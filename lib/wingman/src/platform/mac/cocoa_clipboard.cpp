#ifdef __APPLE__

#include "wingman/platform/iclipboard.hpp"
#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#include <spdlog/spdlog.h>

namespace wingman::platform::mac {

class CocoaClipboard : public IClipboard {
public:
    CocoaClipboard() = default;
    ~CocoaClipboard() override { shutdown(); }

    bool initialize() override {
        initialized_ = true;
        return true;
    }

    void shutdown() override {
        initialized_ = false;
    }

    bool setText(const std::string& text) override {
        if (!initialized_) return false;
        @autoreleasepool {
            NSPasteboard* pb = [NSPasteboard generalPasteboard];
            [pb clearContents];
            NSString* nsText = [NSString stringWithUTF8String:text.c_str()];
            return [pb setString:nsText forType:NSPasteboardTypeString] == YES;
        }
    }

    std::string getText() override {
        if (!initialized_) return "";
        @autoreleasepool {
            NSPasteboard* pb = [NSPasteboard generalPasteboard];
            NSString* nsText = [pb stringForType:NSPasteboardTypeString];
            if (nsText) {
                return [nsText UTF8String];
            }
            return "";
        }
    }

    bool hasText() override {
        if (!initialized_) return false;
        @autoreleasepool {
            NSPasteboard* pb = [NSPasteboard generalPasteboard];
            return [pb canReadObjectForClasses:@[[NSString class]] options:nil] == YES;
        }
    }

    bool setHTML(const std::string& html) override {
        if (!initialized_) return false;
        @autoreleasepool {
            NSPasteboard* pb = [NSPasteboard generalPasteboard];
            [pb clearContents];
            NSString* nsHtml = [NSString stringWithUTF8String:html.c_str()];
            NSData* data = [nsHtml dataUsingEncoding:NSUTF8StringEncoding];
            [pb setData:data forType:NSPasteboardTypeHTML];
            return true;
        }
    }

    std::string getHTML() override {
        if (!initialized_) return "";
        @autoreleasepool {
            NSPasteboard* pb = [NSPasteboard generalPasteboard];
            NSData* data = [pb dataForType:NSPasteboardTypeHTML];
            if (data) {
                NSString* nsStr = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
                if (nsStr) return [nsStr UTF8String];
            }
            return "";
        }
    }

    bool hasHTML() override {
        if (!initialized_) return false;
        @autoreleasepool {
            NSPasteboard* pb = [NSPasteboard generalPasteboard];
            return [[pb types] containsObject:NSPasteboardTypeHTML] == YES;
        }
    }

    bool setImage(const std::vector<uint8_t>& imageData, int width, int height) override {
        if (!initialized_ || imageData.empty()) return false;
        @autoreleasepool {
            CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
            CGContextRef ctx = CGBitmapContextCreate(
                (void*)imageData.data(), width, height, 8, width * 4,
                colorSpace, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);
            if (!ctx) { CGColorSpaceRelease(colorSpace); return false; }

            CGImageRef cgImage = CGBitmapContextCreateImage(ctx);
            CGContextRelease(ctx);
            CGColorSpaceRelease(colorSpace);
            if (!cgImage) return false;

            NSImage* nsImage = [[NSImage alloc] initWithCGImage:cgImage
                                                          size:NSMakeSize(width, height)];
            CGImageRelease(cgImage);

            NSPasteboard* pb = [NSPasteboard generalPasteboard];
            [pb clearContents];
            bool ok = [pb writeObjects:@[nsImage]] == YES;
            return ok;
        }
    }

    std::vector<uint8_t> getImage(int* outWidth, int* outHeight) override {
        if (outWidth) *outWidth = 0;
        if (outHeight) *outHeight = 0;
        if (!initialized_) return {};
        @autoreleasepool {
            NSPasteboard* pb = [NSPasteboard generalPasteboard];
            NSArray* images = [pb readObjectsForClasses:@[[NSImage class]] options:nil];
            if (!images || images.count == 0) return {};

            NSImage* nsImage = images[0];
            CGImageRef cgImage = [nsImage CGImageForProposedRect:nil context:nil hints:nil];
            if (!cgImage) return {};

            size_t w = CGImageGetWidth(cgImage);
            size_t h = CGImageGetHeight(cgImage);
            if (outWidth) *outWidth = static_cast<int>(w);
            if (outHeight) *outHeight = static_cast<int>(h);

            std::vector<uint8_t> data(w * h * 4);
            CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
            CGContextRef ctx = CGBitmapContextCreate(
                data.data(), w, h, 8, w * 4,
                colorSpace, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);
            if (ctx) {
                CGContextDrawImage(ctx, CGRectMake(0, 0, w, h), cgImage);
                CGContextRelease(ctx);
            }
            CGColorSpaceRelease(colorSpace);
            return data;
        }
    }

    bool hasImage() override {
        if (!initialized_) return false;
        @autoreleasepool {
            NSPasteboard* pb = [NSPasteboard generalPasteboard];
            return [pb canReadObjectForClasses:@[[NSImage class]] options:nil] == YES;
        }
    }

    bool setFiles(const std::vector<std::string>& files) override {
        if (!initialized_ || files.empty()) return false;
        @autoreleasepool {
            NSMutableArray* urls = [NSMutableArray arrayWithCapacity:files.size()];
            for (const auto& f : files) {
                NSString* path = [NSString stringWithUTF8String:f.c_str()];
                NSURL* url = [NSURL fileURLWithPath:path];
                [urls addObject:url];
            }
            NSPasteboard* pb = [NSPasteboard generalPasteboard];
            [pb clearContents];
            return [pb writeObjects:urls] == YES;
        }
    }

    std::vector<std::string> getFiles() override {
        if (!initialized_) return {};
        @autoreleasepool {
            NSPasteboard* pb = [NSPasteboard generalPasteboard];
            NSArray* urls = [pb readObjectsForClasses:@[[NSURL class]] options:nil];
            std::vector<std::string> result;
            for (NSURL* url in urls) {
                if ([url isFileURL]) {
                    result.push_back([[url path] UTF8String]);
                }
            }
            return result;
        }
    }

    bool hasFiles() override {
        if (!initialized_) return false;
        @autoreleasepool {
            NSPasteboard* pb = [NSPasteboard generalPasteboard];
            return [pb canReadObjectForClasses:@[[NSURL class]] options:nil] == YES;
        }
    }

    void clear() override {
        @autoreleasepool {
            [[NSPasteboard generalPasteboard] clearContents];
        }
    }

    bool isEmpty() override {
        @autoreleasepool {
            return [[[NSPasteboard generalPasteboard] types] count] == 0;
        }
    }

    std::vector<ClipboardFormat> getAvailableFormats() override {
        std::vector<ClipboardFormat> formats;
        if (hasText()) formats.push_back(ClipboardFormat::Text);
        if (hasHTML()) formats.push_back(ClipboardFormat::HTML);
        if (hasImage()) formats.push_back(ClipboardFormat::Image);
        if (hasFiles()) formats.push_back(ClipboardFormat::Files);
        return formats;
    }

    std::string getBackendName() const override { return "NSPasteboard"; }
    BackendInfo getBackendInfo() const override {
        return {"NSPasteboard", "1.0", initialized_, "macOS Cocoa Clipboard"};
    }

private:
    bool initialized_ = false;
};

} // namespace wingman::platform::mac

#endif // __APPLE__
