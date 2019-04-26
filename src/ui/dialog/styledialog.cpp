// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief A dialog for CSS selectors
 */
/* Authors:
 *   Kamalpreet Kaur Grewal
 *   Tavmjong Bah
 *
 * Copyright (C) Kamalpreet Kaur Grewal 2016 <grewalkamal005@gmail.com>
 * Copyright (C) Tavmjong Bah 2017 <tavmjong@free.fr>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "styledialog.h"
#include "verbs.h"
#include "selection.h"
#include "attribute-rel-svg.h"
#include "inkscape.h"
#include "document-undo.h"

#include "ui/icon-loader.h"
#include "ui/widget/iconrenderer.h"

#include "xml/attribute-record.h"
#include "xml/node-observer.h"

#include <glibmm/i18n.h>
#include <glibmm/regex.h>

#include <map>
#include <utility>

//#define DEBUG_STYLEDIALOG
//#define G_LOG_DOMAIN "STYLEDIALOG"

using Inkscape::DocumentUndo;
using Inkscape::Util::List;
using Inkscape::XML::AttributeRecord;

/**
 * This macro is used to remove spaces around selectors or any strings when
 * parsing is done to update XML style element or row labels in this dialog.
 */
#define REMOVE_SPACES(x) x.erase(0, x.find_first_not_of(' ')); \
    x.erase(x.find_last_not_of(' ') + 1);

namespace Inkscape {
namespace UI {
namespace Dialog {

// Keeps a watch on style element
class StyleDialog::NodeObserver : public Inkscape::XML::NodeObserver {
public:
    NodeObserver(StyleDialog* styledialog) :
        _styledialog(styledialog)
    {
        g_debug("StyleDialog::NodeObserver: Constructor");
    };

    void notifyContentChanged(Inkscape::XML::Node &node,
                                      Inkscape::Util::ptr_shared old_content,
                                      Inkscape::Util::ptr_shared new_content) override;

    StyleDialog * _styledialog;
};


void
StyleDialog::NodeObserver::notifyContentChanged(
    Inkscape::XML::Node &/*node*/,
    Inkscape::Util::ptr_shared /*old_content*/,
    Inkscape::Util::ptr_shared /*new_content*/ ) {

    g_debug("StyleDialog::NodeObserver::notifyContentChanged");
    _styledialog->_updating = false;
    _styledialog->_readStyleElement();
    _styledialog->_filterRow();
}


// Keeps a watch for new/removed/changed nodes
// (Must update objects that selectors match.)
class StyleDialog::NodeWatcher : public Inkscape::XML::NodeObserver {
public:
    NodeWatcher(StyleDialog* styledialog, Inkscape::XML::Node *repr) :
        _styledialog(styledialog),
        _repr(repr)
    {
        g_debug("StyleDialog::NodeWatcher: Constructor");
    };

    void notifyChildAdded( Inkscape::XML::Node &/*node*/,
                                   Inkscape::XML::Node &child,
                                   Inkscape::XML::Node */*prev*/ ) override
    {
        if ( _styledialog && _repr ) {
            _styledialog->_nodeAdded( child );
        }
    }

    void notifyChildRemoved( Inkscape::XML::Node &/*node*/,
                                     Inkscape::XML::Node &child,
                                     Inkscape::XML::Node */*prev*/ ) override
    {
        if ( _styledialog && _repr ) {
            _styledialog->_nodeRemoved( child );
        }
    }

    void notifyAttributeChanged( Inkscape::XML::Node &node,
                                         GQuark qname,
                                         Util::ptr_shared /*old_value*/,
                                         Util::ptr_shared /*new_value*/ ) override {
        if ( _styledialog && _repr ) {

            // For the moment only care about attributes that are directly used in selectors.
            const gchar * cname = g_quark_to_string (qname );
            Glib::ustring name;
            if (cname) {
                name = cname;
            }

            if ( name == "id" || name == "class" ) {
                _styledialog->_nodeChanged( node );
            }
        }
    }

    StyleDialog * _styledialog;
    Inkscape::XML::Node * _repr;  // Need to track if document changes.
};

void
StyleDialog::_nodeAdded( Inkscape::XML::Node &node ) {

    StyleDialog::NodeWatcher *w = new StyleDialog::NodeWatcher (this, &node);
    node.addObserver (*w);
    _nodeWatchers.push_back(w);

    _readStyleElement();
    _filterRow();
}

void
StyleDialog::_nodeRemoved( Inkscape::XML::Node &repr ) {

    for (auto it = _nodeWatchers.begin(); it != _nodeWatchers.end(); ++it) {
        if ( (*it)->_repr == &repr ) {
            (*it)->_repr->removeObserver (**it);
            _nodeWatchers.erase( it );
            break;
        }
    }

    _readStyleElement();
    _filterRow();
}

void
StyleDialog::_nodeChanged( Inkscape::XML::Node &object ) {

    _readStyleElement();
    _filterRow();
}

StyleDialog::TreeStore::TreeStore()
= default;


// This is only here to handle updating style element after a drag and drop.
void
StyleDialog::TreeStore::on_row_deleted(const TreeModel::Path& path)
{
    if (_styledialog->_updating) return;  // Don't write if we deleted row (other than from DND)

    g_debug("on_row_deleted");

    _styledialog->_writeStyleElement();
}


Glib::RefPtr<StyleDialog::TreeStore> StyleDialog::TreeStore::create(StyleDialog *styledialog)
{
    StyleDialog::TreeStore * store = new StyleDialog::TreeStore();
    store->_styledialog = styledialog;
    store->set_column_types( store->_styledialog->_mColumns );
    return Glib::RefPtr<StyleDialog::TreeStore>( store );
}

/**
 * Constructor
 * A treeview and a set of two buttons are added to the dialog. _addSelector
 * adds selectors to treeview. _delSelector deletes the selector from the dialog.
 * Any addition/deletion of the selectors updates XML style element accordingly.
 */
StyleDialog::StyleDialog() :
    UI::Widget::Panel("/dialogs/style", SP_VERB_DIALOG_STYLE),
    _updating(false),
    _textNode(nullptr),
    _desktopTracker()
{
    g_debug("StyleDialog::StyleDialog");
    _store = TreeStore::create(this);
    _modelfilter = Gtk::TreeModelFilter::create(_store);
    _modelfilter->set_visible_column(_mColumns._colVisible);
    _treeView.set_model(_modelfilter);
    _treeView.set_headers_visible(false);
    _treeView.set_grid_lines (Gtk::TREE_VIEW_GRID_LINES_HORIZONTAL);
    _treeView.enable_model_drag_source();
    _treeView.enable_model_drag_dest( Gdk::ACTION_MOVE );
    _treeView.append_column("CSS Selector", _mColumns._colData);

    // Pack widgets
    _paned.set_orientation(Gtk::ORIENTATION_VERTICAL);
    _paned.pack1(_mainBox, Gtk::SHRINK);
    _mainBox.set_orientation(Gtk::ORIENTATION_VERTICAL);

    _mainBox.pack_start(_treeView, Gtk::PACK_EXPAND_WIDGET);
    _getContents()->pack_start(_paned, Gtk::PACK_EXPAND_WIDGET);


    // Document & Desktop
    _desktop_changed_connection = _desktopTracker.connectDesktopChanged(
        sigc::mem_fun(*this, &StyleDialog::_handleDesktopChanged) );
    _desktopTracker.connect(GTK_WIDGET(gobj()));

    _document_replaced_connection = getDesktop()->connectDocumentReplaced(
        sigc::mem_fun(this, &StyleDialog::_handleDocumentReplaced));

    _selection_changed_connection = getDesktop()->getSelection()->connectChanged(
        sigc::hide(sigc::mem_fun(this, &StyleDialog::_handleSelectionChanged)));

    // Add watchers
    _updateWatchers();

    // Load tree
    _readStyleElement();
    _filterRow();
}


/**
 * Class destructor
 */
StyleDialog::~StyleDialog()
{
    g_debug("StyleDialog::~StyleDialog");
    _desktop_changed_connection.disconnect();
    _document_replaced_connection.disconnect();
    _selection_changed_connection.disconnect();
}


/**
 * @return Inkscape::XML::Node* pointing to a style element's text node.
 * Returns the style element's text node. If there is no style element, one is created.
 * Ditto for text node.
 */
Inkscape::XML::Node* StyleDialog::_getStyleTextNode()
{

    Inkscape::XML::Node *styleNode = nullptr;
    Inkscape::XML::Node *textNode = nullptr;

    Inkscape::XML::Node *root = SP_ACTIVE_DOCUMENT->getReprRoot();
    for (unsigned i = 0; i < root->childCount(); ++i) {
        if (Glib::ustring(root->nthChild(i)->name()) == "svg:style") {

            styleNode = root->nthChild(i);

            for (unsigned j = 0; j < styleNode->childCount(); ++j) {
                if (styleNode->nthChild(j)->type() == Inkscape::XML::TEXT_NODE) {
                    textNode = styleNode->nthChild(j);
                }
            }

            if (textNode == nullptr) {
                // Style element found but does not contain text node!
                std::cerr << "StyleDialog::_getStyleTextNode(): No text node!" << std::endl;
                textNode = SP_ACTIVE_DOCUMENT->getReprDoc()->createTextNode("");
                styleNode->appendChild(textNode);
                Inkscape::GC::release(textNode);
            }
        }
    }

    if (styleNode == nullptr) {
        // Style element not found, create one
        styleNode = SP_ACTIVE_DOCUMENT->getReprDoc()->createElement("svg:style");
        textNode  = SP_ACTIVE_DOCUMENT->getReprDoc()->createTextNode("");

        styleNode->appendChild(textNode);
        Inkscape::GC::release(textNode);

        root->addChild(styleNode, nullptr);
        Inkscape::GC::release(styleNode);
    }

    if (_textNode != textNode) {
        _textNode = textNode;
        NodeObserver *no = new NodeObserver(this);
        textNode->addObserver(*no);
    }

    return textNode;
}


/**
 * Fill the Gtk::TreeStore from the svg:style element.
 */
void StyleDialog::_readStyleElement()
{
    g_debug("StyleDialog::_readStyleElement: updating %s", (_updating ? "true" : "false"));

    if (_updating) return; // Don't read if we wrote style element.
    _updating = true;

    Inkscape::XML::Node * textNode = _getStyleTextNode();
    if (textNode == nullptr) {
        std::cerr << "StyleDialog::_readStyleElement: No text node!" << std::endl;
    }

    // Get content from style text node.
    std::string content = (textNode->content() ? textNode->content() : "");

    // Remove end-of-lines (check it works on Windoze).
    content.erase(std::remove(content.begin(), content.end(), '\n'), content.end());

    // Remove comments (/* xxx */)
    while(content.find("/*") != std::string::npos) {
        size_t start = content.find("/*");
        content.erase(start, (content.find("*/", start) - start) +2);
    }

    // First split into selector/value chunks.
    // An attempt to use Glib::Regex failed. A C++11 version worked but
    // reportedly has problems on Windows. Using split_simple() is simpler
    // and probably faster.
    //
    // Glib::RefPtr<Glib::Regex> regex1 =
    //   Glib::Regex::create("([^\\{]+)\\{([^\\{]+)\\}");
    //
    // Glib::MatchInfo minfo;
    // regex1->match(content, minfo);

    // Split on curly brackets. Even tokens are selectors, odd are values.
    std::vector<Glib::ustring> tokens = Glib::Regex::split_simple("[}{]", content);

    // If text node is empty, return (avoids problem with negative below).
    if (tokens.size() == 0) {
        _updating = false;
        return;
    }
    _store->clear();

    for (unsigned i = 0; i < tokens.size()-1; i += 2) {

        Glib::ustring selector = tokens[i];
        REMOVE_SPACES(selector); // Remove leading/trailing spaces
        std::vector<Glib::ustring> tokensplus = Glib::Regex::split_simple("[,]+", selector);
        for (auto tok : tokensplus) {
            REMOVE_SPACES(tok);
/*             if (tok.find(" ") != -1 || tok.erase(0, 1).find(".") != -1) {
                colType = UNHANDLED;
            } */
        }
        // Get list of objects selector matches
        std::vector<SPObject *> objVec = _getObjVec( selector );

        Glib::ustring properties;
        // Check to make sure we do have a value to match selector.
        if ((i+1) < tokens.size()) {
            properties = tokens[i+1];
        } else {
            std::cerr << "StyleDialog::_readStyleElement: Missing values "
                "for last selector!" << std::endl;
        }
        Gtk::TreeModel::Row row = *(_store->append());
        row[_mColumns._colData]    = selector;
        row[_mColumns._colObj]     = objVec;
        row[_mColumns._colVisible] = true;
        row[_mColumns._colProperties] = properties;
        std::vector<Glib::ustring> properties_data = Glib::Regex::split_simple(";", properties);
        for (auto property : properties_data) {
            property = REMOVE_SPACES(property);
            Gtk::TreeModel::Row childrow = *(_store->append(row->children()));
            childrow[_mColumns._colData] = Glib::ustring(property);
            childrow[_mColumns._colObj] = {};
            childrow[_mColumns._colProperties] = ""; // Unused
            childrow[_mColumns._colVisible] = true; // Unused
        }
    }
    _updating = false;
}

/**
 * Update the content of the style element as selectors (or objects) are added/removed.
 */
void StyleDialog::_writeStyleElement()
{
    if (_updating) {
        return;
    }
    _updating = true;

    Glib::ustring styleContent;
    for (auto& row: _store->children()) {
        Glib::ustring selector = row[_mColumns._colData];
        /*
                REMOVE_SPACES(selector);
        /*         size_t len = selector.size();
                if(selector[len-1] == ','){
                    selector.erase(len-1);
                }
                row[_mColumns._colData] =  selector; */
        styleContent = styleContent + selector + " { " + row[_mColumns._colProperties] + " }\n";
    }
    // We could test if styleContent is empty and then delete the style node here but there is no
    // harm in keeping it around ...

    Inkscape::XML::Node *textNode = _getStyleTextNode();
    textNode->setContent(styleContent.c_str());

    DocumentUndo::done(SP_ACTIVE_DOCUMENT, SP_VERB_DIALOG_STYLE, _("Edited style element."));

    _updating = false;
    g_debug("StyleDialog::_writeStyleElement(): | %s |", styleContent.c_str());
}


void StyleDialog::_addWatcherRecursive(Inkscape::XML::Node *node) {

    g_debug("StyleDialog::_addWatcherRecursive()");

    StyleDialog::NodeWatcher *w = new StyleDialog::NodeWatcher(this, node);
    node->addObserver(*w);
    _nodeWatchers.push_back(w);

    for (unsigned i = 0; i < node->childCount(); ++i) {
        _addWatcherRecursive(node->nthChild(i));
    }
}

/**
 * Update the watchers on objects.
 */
void StyleDialog::_updateWatchers()
{
    _updating = true;

    // Remove old document watchers
    while (!_nodeWatchers.empty()) {
        StyleDialog::NodeWatcher *w = _nodeWatchers.back();
        w->_repr->removeObserver(*w);
        _nodeWatchers.pop_back();
        delete w;
    }

    // Recursively add new watchers
    Inkscape::XML::Node *root = SP_ACTIVE_DOCUMENT->getReprRoot();
    _addWatcherRecursive(root);

    g_debug("StyleDialog::_updateWatchers(): %d", (int)_nodeWatchers.size());

    _updating = false;
}


/**
 * @param sel
 * @return This function returns a comma separated list of ids for objects in input vector.
 * It is used in creating an 'id' selector. It relies on objects having 'id's.
 */
Glib::ustring StyleDialog::_getIdList(std::vector<SPObject*> sel)
{
    Glib::ustring str;
    for (auto& obj: sel) {
        str += "#" + Glib::ustring(obj->getId()) + ", ";
    }
    if (!str.empty()) {
        str.erase(str.size()-1); // Remove space at end. c++11 has pop_back() but not ustring.
        str.erase(str.size()-1); // Remove comma at end.
    }
    return str;
}

/**
 * @param selector: a valid CSS selector string.
 * @return objVec: a vector of pointers to SPObject's the selector matches.
 * Return a vector of all objects that selector matches.
 */
std::vector<SPObject *> StyleDialog::_getObjVec(Glib::ustring selector) {

    std::vector<SPObject *> objVec = SP_ACTIVE_DOCUMENT->getObjectsBySelector( selector );

    g_debug("StyleDialog::_getObjVec: | %s |", selector.c_str());
    for (auto& obj: objVec) {
        g_debug("  %s", obj->getId() ? obj->getId() : "null");
    }

    return objVec;
}

void StyleDialog::_closeDialog(Gtk::Dialog *textDialogPtr) { textDialogPtr->response(Gtk::RESPONSE_OK); }

// -------------------------------------------------------------------

class PropertyData
{
public:
    PropertyData() = default;;
    PropertyData(Glib::ustring name) : _name(std::move(name)) {};

    void _setSheetValue(Glib::ustring value) { _sheetValue = value; };
    void _setAttrValue(Glib::ustring value)  { _attrValue  = value; };
    Glib::ustring _getName()       { return _name;       };
    Glib::ustring _getSheetValue() { return _sheetValue; };
    Glib::ustring _getAttrValue()  { return _attrValue;  };

private:
    Glib::ustring _name;
    Glib::ustring _sheetValue;
    Glib::ustring _attrValue;
};

// -------------------------------------------------------------------


/**
 * Handle document replaced. (Happens when a default document is immediately replaced by another
 * document in a new window.)
 */
void
StyleDialog::_handleDocumentReplaced(SPDesktop *desktop, SPDocument * /* document */)
{
    g_debug("StyleDialog::handleDocumentReplaced()");

    _selection_changed_connection.disconnect();

    _selection_changed_connection = desktop->getSelection()->connectChanged(
        sigc::hide(sigc::mem_fun(this, &StyleDialog::_handleSelectionChanged)));

    _updateWatchers();
    _readStyleElement();
    _filterRow();
}


/*
 * When a dialog is floating, it is connected to the active desktop.
 */
void
StyleDialog::_handleDesktopChanged(SPDesktop* desktop) {
    g_debug("StyleDialog::handleDesktopReplaced()");

    if (getDesktop() == desktop) {
        // This will happen after construction of dialog. We've already
        // set up signals so just return.
        return;
    }

    _selection_changed_connection.disconnect();
    _document_replaced_connection.disconnect();

    setDesktop( desktop );

    _selection_changed_connection = desktop->getSelection()->connectChanged(
        sigc::hide(sigc::mem_fun(this, &StyleDialog::_handleSelectionChanged)));
    _document_replaced_connection = desktop->connectDocumentReplaced(
        sigc::mem_fun(this, &StyleDialog::_handleDocumentReplaced));

    _updateWatchers();
    _readStyleElement();
    _filterRow();
}


/*
 * Handle a change in which objects are selected in a document.
 */
void
StyleDialog::_handleSelectionChanged() {
    g_debug("StyleDialog::_handleSelectionChanged()");
    _filterRow();
}

/**
 * This function selects the row in treeview corresponding to an object selected
 * in the drawing. If more than one row matches, the first is chosen.
 */
void StyleDialog::_filterRow()
{
    g_debug("StyleDialog::_selectRow: updating: %s", (_updating ? "true" : "false"));
    if (_updating || !getDesktop()) return; // Avoid updating if we have set row via dialog.
    if (SP_ACTIVE_DESKTOP != getDesktop()) {
        std::cerr << "StyleDialog::_selectRow: SP_ACTIVE_DESKTOP != getDesktop()" << std::endl;
        return;
    }
    Gtk::TreeModel::Children children = _store->children();
    Inkscape::Selection* selection = getDesktop()->getSelection();
    SPObject *obj = nullptr;
    if(selection->objects().size() == 1) {
        obj = selection->objects().back();
    }
    for (auto row : children) {
        std::vector<SPObject *> objVec = row[_mColumns._colObj];
        if (obj) {
            for (auto & i : objVec) {
                if (obj->getId() == i->getId()) {
                    row[_mColumns._colVisible] = true;
                    break;
                } else {
                    row[_mColumns._colVisible] = false;
                }
            }
        }
    }
    _modelfilter->refilter();
}

} // namespace Dialog
} // namespace UI
} // namespace Inkscape

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
