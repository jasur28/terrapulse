#pragma once

#include <QString>

namespace tp::io {

class GreenFunctionArchive {
public:
    virtual ~GreenFunctionArchive() = default;
    virtual bool setSource(const QString& source) = 0;
};

} // namespace tp::io
