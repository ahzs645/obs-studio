#ifdef __APPLE__
#import <ApplicationServices/ApplicationServices.h>

bool check_screen_permission()
{
    return CGPreflightScreenCaptureAccess();
}

bool request_screen_permission()
{
    return CGRequestScreenCaptureAccess();
}

#else

bool check_screen_permission()
{
    return true;
}

bool request_screen_permission()
{
    return true;
}
#endif
