#ifndef DBINIT_H
#define DBINIT_H

#include <string>

namespace DBInit {

bool initializeSchema(const std::string& conninfo, const std::string& sqlFilePath);

} // namespace DBInit

#endif // DBINIT_H
