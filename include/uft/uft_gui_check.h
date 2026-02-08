/**
 * @file uft_gui_check.h
 * @brief Qt Availability Check
 * 
 * Include this header to check if Qt GUI support is available.
 */

#ifndef UFT_GUI_CHECK_H
#define UFT_GUI_CHECK_H

/* Check for Qt availability */
#if defined(QT_VERSION) || defined(Q_MOC_OUTPUT_REVISION)
    #define UFT_HAS_QT 1
#else
    #define UFT_HAS_QT 0
#endif

#if UFT_HAS_QT
    /* Qt is available - GUI headers can be used */
    #define UFT_GUI_AVAILABLE 1
#else
    /* No Qt - use command-line interface only */
    #define UFT_GUI_AVAILABLE 0
#endif

#endif /* UFT_GUI_CHECK_H */
