#ifndef CORE_PLAINAPP_H
#define CORE_PLAINAPP_H

#include "core/IApp.h"

#include <memory>

class PlainApp : public IApp {
   public:
    PlainApp();
    UwsApp& uws() override;
    void defer(DeferFn fn) override;
};

#endif  // CORE_PLAINAPP_H
