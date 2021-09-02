#include "HealthService.h"
#include "ananas/util/Buffer.h"
#include "ananas/util/HttpProtocol.h"
#include "ProtobufCoder.h"
#include "RpcService.h"
#include "RpcServer.h"

namespace ananas {

namespace rpc {


//aqua, black, blue, fuchsia, gray, green, lime, maroon,
//navy, olive, purple, red, silver, teal, white, yellow
static
std::string ColorWord(const std::string& text, const std::string& color) {
    return "<div style=\"color:" + color + "\">" + text + "</div>";
}

#define NEWLINE "<br>"

void HealthServiceImpl::GetSummary(::google::protobuf::RpcController*,
                                   const rpc::HttpRequestMsg* request,
                                   rpc::Summary* response,
                                   ::google::protobuf::Closure* done) {

    std::string* summary = response->mutable_summary();
    summary->reserve(256);
    summary->append("HTTP/1.1 200 OK\r\n");

    std::string clen("Content-Length: ");
    const std::string type("Content-Type: text/html");
    std::string html("<!DOCTYPE html> \n"
                        "<html>\n \
                            <head>\n \
                                <meta charset=\"UTF-8\">\n \
                                <title>Welcome to ananas rpc</title> \n \
                                <style> div{display:inline} </style> \n \
                            </head>\n \
                            <body>\n \
                                ANANAS: To be continued <br><br>");
    // workers;
    html += ColorWord("<b>Worker threads: " + std::to_string(RPC_SERVER.NumOfWorker()) + "</b>", "maroon");
    html += NEWLINE, html += NEWLINE;
    html += ColorWord("<b>Services:</b>", "fuchsia");
    html += NEWLINE;

    // service table
    html += "<table border=\"1\"> <tr> <th>Service</th> <th>Address</th> <th>Connections</th>";
    for (const auto& kv : RPC_SERVER.services_) {
        size_t connections = 0;
        for (const auto& cmap : kv.second->channels_)
            connections += cmap.size();

        std::string name(kv.first.ToString(), 0, 30);
        html += "<tr> <td>" + ColorWord(name, "teal") + "</td>" +
                 "<td>" + ColorWord(EndpointToString(kv.second->GetEndpoint()), "Blue") + "</td>" +
                 "<td>" + ColorWord(std::to_string(connections), "Green") + "</td>" +
                 "</tr>";
    }
    html += "</table>";

    html += "</body>\n</html>";
    clen += std::to_string(html.size());

    summary->append(clen);
    summary->append("\r\n");
    summary->append(type);
    summary->append("\r\n\r\n");

    summary->append(html);
    summary->append("\r\n");

    done->Run();
}

static
bool M2FEncode(const google::protobuf::Message* msg, rpc::RpcMessage& frame) {
    // msg is Summary
    auto summary = dynamic_cast<const Summary*>(msg);
    assert (summary);

    std::string* result = frame.mutable_response()->mutable_serialized_response();
    *result = summary->summary();
    return true;
}

static
std::shared_ptr<google::protobuf::Message> B2MDecode(HttpRequestParser* parser, const char*& data, size_t len) {
    while (!parser->Done()) {
        if (!parser->ParseRequest(data, data+len))
            throw Exception(ErrorCode::DecodeFail);
        else if (parser->WaitMore())
            return std::shared_ptr<google::protobuf::Message>();
    }

    auto msg = std::make_shared<HttpRequestMsg>();
    if (parser->Done()) {
        const auto& req = parser->Request();

        msg->mutable_method()->append(req.MethodString());
        msg->mutable_path()->append(req.Path());
        // TODO headers
        msg->mutable_body()->append(req.Body());
        parser->Reset();
    }

    return msg;
}

void OnCreateHealthChannel(rpc::ServerChannel* c) {
    auto ctx = std::make_shared<HttpRequestParser>();
    c->SetContext(ctx);

    rpc::Encoder encoder;
    encoder.SetMessageToFrameEncoder(M2FEncode);
    c->SetEncoder(std::move(encoder));

    rpc::Decoder decoder;
    decoder.SetBytesToMessageDecoder(std::bind(B2MDecode,
                                     ctx.get(),
                                     std::placeholders::_1,
                                     std::placeholders::_2));
    c->SetDecoder(std::move(decoder));
}

std::string DispatchHealthMethod(const ::google::protobuf::Message* ) {
    return "GetSummary";
}

} // end namespace rpc

} // end namespace ananas

