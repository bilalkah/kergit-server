#ifndef CORE_PLAINAPP_H
#define CORE_PLAINAPP_H

#include "core/IApp.h"
#include "utils/Loggable.h"

#include <memory>

namespace core {

class PlainApp : public IApp, utils::Loggable {
   public:
    PlainApp() : app_() {
        log(utils::LogLevel::WARN, "PlainApp created. SSL ", kSslEnabled ? "enabled" : "disabled");
    }
    UwsApp& uws() override { return app_; }
    void defer(DeferFn fn) override {
        // Post onto the current loop (callable from within the loop)
        uWS::Loop::get()->defer(std::move(fn));
    }

   private:
    UwsApp app_;
};

}  // namespace core

#endif  // CORE_PLAINAPP_H
