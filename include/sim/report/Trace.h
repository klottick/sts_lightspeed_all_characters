#ifndef STS_REPORT_TRACE_H
#define STS_REPORT_TRACE_H

#include <nlohmann/json.hpp>
#include <iosfwd>
#include <string>

namespace sts::report {
using Json = nlohmann::json;

class TraceWriter {
public:
    TraceWriter(std::ostream& out, const Json& run);
    void appendCatalog(const Json& entries);
    void appendFrame(const Json& frame);
    void finish(const std::string& status);
private:
    void write(const Json& record);
    std::ostream& out_;
    std::uint64_t nextId_ = 0;
    bool finished_ = false;
};

Json readTrace(std::istream& input);
}
#endif
