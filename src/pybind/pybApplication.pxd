from libcpp.list cimport list as cpplist
from pybSPDesktop cimport SPDesktop#, res_slot_type
ctypedef SPDesktop *SPDesktop_p

#cdef extern from "sigc++/signal.h" namespace "sigc":
#    cdef cppclass signal[T_return, T_arg1]:
#        void connect(res_slot_type slot_type)

cdef extern from "inkscape.h" namespace "Inkscape":
    cdef cppclass Application:
        @staticmethod
        Application& instance()

        void activate_desktop (SPDesktop * desktop)
        void switch_desktops_next ()
        void switch_desktops_prev ()
        void get_all_desktops (cpplist[SPDesktop_p] & listbuf)
    
        void dialogs_hide()
        void dialogs_unhide ()
        void dialogs_toggle ()
    
        void external_change ()
        void subselection_changed (SPDesktop *desktop)
    
        void refresh_display ()
        void exit ()

        #signal[void, SPDesktop *] signal_activate_desktop;
