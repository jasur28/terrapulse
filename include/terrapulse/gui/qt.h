#pragma once

#if defined(_WIN32) && (defined(TP_GUI_SHARED) || defined(TP_ALL_SHARED))
# if defined(TP_GUI_EXPORTS)
#  define TP_GUI_API __declspec(dllexport)
#  define TP_GUI_TEMPLATE_EXPORT
# else
#  define TP_GUI_API __declspec(dllimport)
#  define TP_GUI_TEMPLATE_EXPORT extern
# endif
#else
# define TP_GUI_API
# define TP_GUI_TEMPLATE_EXPORT
#endif
