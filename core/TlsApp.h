#ifndef CORE_TLSAPP_H
#define CORE_TLSAPP_H

#include "core/IApp.h"
#include "utils/TlsConfig.h"

#include <memory>

class TLSApp : public IApp {
   public:
    explicit TLSApp(const TlsConfig& cfg);
    UwsApp& uws() override;
    void defer(DeferFn fn) override;
};

#endif  // CORE_TLSAPP_H
