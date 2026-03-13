#include "SharedPathUtils.h"
#include <string>

#ifdef __APPLE__
#import <Foundation/Foundation.h>

namespace Mach1 {

// Objective-C++ implementation for App Group container access
std::string getAppGroupContainerImpl(const std::string& groupIdentifier);
std::string getAppGroupContainerImpl(const std::string& groupIdentifier) {
    @autoreleasepool {
        static bool loggedSuccess = false;
        static bool loggedFailure = false;
        NSString* groupId = [NSString stringWithUTF8String:groupIdentifier.c_str()];
        NSURL* containerURL = [[NSFileManager defaultManager] 
            containerURLForSecurityApplicationGroupIdentifier:groupId];
        
        if (containerURL && containerURL.path) {
            std::string result = std::string([containerURL.path UTF8String]);
            if (!loggedSuccess) {
                NSLog(@"[SharedPathUtils] App Group container found: %@", containerURL.path);
                loggedSuccess = true;
            }
            return result;
        }
        
        if (!loggedFailure) {
            NSLog(@"[SharedPathUtils] App Group container not found for: %@", groupId);
            loggedFailure = true;
        }
        return "";
    }
}

} // namespace Mach1

#else

namespace Mach1 {

// Non-Apple platforms
std::string getAppGroupContainerImpl(const std::string& groupIdentifier) {
    return "";
}

} // namespace Mach1

#endif 