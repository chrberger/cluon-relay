/*
 * Copyright (C) 2020  Christian Berger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cluon-complete.hpp"

#include <chrono>
#include <iostream>
#include <thread>

int32_t main(int32_t argc, char **argv) {
  int32_t retCode{0};

  auto usage = [&argv, &retCode](){
    std::cerr << argv[0] << " relays Envelopes from one CID to another CID." << std::endl;
    std::cerr << "Usage:   " << argv[0] << " --cid-from=<source CID> [--via-tcp=<port|ip:port> [--mtu=<MTU>] [--timeout=<Timeout>]] --cid-to=<destination> [--keep=<list of messageIDs to keep>] [--drop=<list of messageIDs to drop>] [--downsampling=<list of messageIDs to downsample>]" << std::endl;
    std::cerr << "         --cid-from:      relay Envelopes originating from this CID" << std::endl;
    std::cerr << "         --cid-to:        relay Envelopes to this CID (must be different from source)" << std::endl;
    std::cerr << "         --via-tcp:       relay Envelopes via a TCP connection; one needs two instances of " << argv[0] << ", where" << std::endl;
    std::cerr << "                          the server (--cid-from) is using --via-tcp=Port (eg., --via-tcp=1234, port > 1023)," << std::endl;
    std::cerr << "                          and the client (--cid-to) is using --via-tcp=IP:Port (eg., --via-tcp=a.b.c.d:1234)." << std::endl;
    std::cerr << "         --mtu:           fill a TCP packet up to this amount instead of sending one for each Envelope; default: 1 (to send for every Envelope)" << std::endl;
    std::cerr << "         --timeout:       send TCP packet after this timeout in ms even if it is not fully filled; default: 1000ms" << std::endl;
    std::cerr << "         --keep:          list of Envelope IDs to keep; example: --keep=19,25" << std::endl;
    std::cerr << "         --drop:          list of Envelope IDs to drop; example: --drop=17,35" << std::endl;
    std::cerr << "         --downsampling:  list of Envelope IDs to downsample; example: --downsample=12:2,31:10  keep every second of 12 and every tenth of 31" << std::endl;
    std::cerr << "                          --keep and --drop must not be used simultaneously." << std::endl;
    std::cerr << "                          Neither specifying --keep, --drop, or --downsample will simply pass all Envelopes from --cid-from to --cid-to." << std::endl;
    std::cerr << "                          Not matching Envelope IDs with --keep are dropped." << std::endl;
    std::cerr << "                          Not matching Envelope IDs with --drop are kept." << std::endl;
    std::cerr << "                          An Envelope IDs with downsampling information supersedes --keep." << std::endl;
    std::cerr << "Examples: " << std::endl;
    std::cerr << "UDP:          " << argv[0] << " --cid-from=111 --cid-to=112 --keep=123" << std::endl;
    std::cerr << "TCP (1-n server): " << argv[0] << " --cid-from=111 --via-tcp=1234 --keep=123" << std::endl;
    std::cerr << "TCP (1-n client): " << argv[0] << " --cid-to=112 --via-tcp=192.168.2.3:1234" << std::endl;
    std::cerr << "TCP (n-1 server): " << argv[0] << " --cid-to=111 --via-tcp=1234 --keep=123" << std::endl;
    std::cerr << "TCP (n-1 client): " << argv[0] << " --cid-from=112 --via-tcp=192.168.2.3:1234" << std::endl;
    retCode = 1;
  };

  auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
  if ((((0 == commandlineArguments.count("cid-from"))
          || (0 == commandlineArguments.count("cid-to")))
        && (0 == commandlineArguments.count("via-tcp")))
      || ((1 == commandlineArguments.count("via-tcp"))
        && ((1 == commandlineArguments.count("cid-from"))
          && (1 == commandlineArguments.count("cid-to"))))
      || ((1 == commandlineArguments.count("cid-from"))
        && (1 == commandlineArguments.count("cid-to"))
        && (commandlineArguments["cid-from"] == commandlineArguments["cid-to"]))
      || ((1 == commandlineArguments.count("keep"))
        && (1 == commandlineArguments.count("drop")))) {
    usage();
  } else {
    std::unordered_map<int32_t, bool, cluon::UseUInt32ValueAsHashKey> mapOfEnvelopesToKeep{};
    {
      std::string tmp{commandlineArguments["keep"]};
      if (!tmp.empty()) {
        tmp += ",";
        auto entries = stringtoolbox::split(tmp, ',');
        for (auto e : entries) {
          std::clog << argv[0] << " keeping " << e << std::endl;
          mapOfEnvelopesToKeep[std::stoi(e)] = true;
        }
      }
    }
    std::unordered_map<int32_t, bool, cluon::UseUInt32ValueAsHashKey> mapOfEnvelopesToDrop{};
    {
      std::string tmp{commandlineArguments["drop"]};
      if (!tmp.empty()) {
        tmp += ",";
        auto entries = stringtoolbox::split(tmp, ',');
        for (auto e : entries) {
          std::clog << argv[0] << " dropping " << e << std::endl;
          mapOfEnvelopesToDrop[std::stoi(e)] = true;
        }
      }
    }

    std::unordered_map<int32_t, uint32_t, cluon::UseUInt32ValueAsHashKey> downsampling{};
    std::unordered_map<int32_t, uint32_t, cluon::UseUInt32ValueAsHashKey> downsamplingCounter{};
    {
      std::string tmp{commandlineArguments["downsample"]};
      if (!tmp.empty()) {
        tmp += ",";
        auto entries = stringtoolbox::split(tmp, ',');
        for (auto e : entries) {
          auto l = stringtoolbox::split(e, ':');
          if ( (2 == l.size()) && (std::stoi(l[1]) > 0) ) {
            std::clog << argv[0] << " using every " << l[1] << "-th Envelope with id " << l[0] << std::endl;
            downsampling[std::stoi(l[0])] = std::stoi(l[1]);
            downsamplingCounter[std::stoi(l[0])] = std::stoi(l[1]);
          }
        }
      }
    }

    const bool VIA_TCP{commandlineArguments.count("via-tcp") != 0};
    if (VIA_TCP) {
      const std::string TCP{commandlineArguments["via-tcp"]};
      const uint32_t MTU{(0 < commandlineArguments.count("mtu")) ? static_cast<uint32_t>(std::stoi(commandlineArguments["mtu"])) : 1};
      uint32_t TIMEOUT{(0 < commandlineArguments.count("timeout")) ? static_cast<uint32_t>(std::stoi(commandlineArguments["timeout"])) : 1000};
      TIMEOUT = (0 == TIMEOUT) ? 1 : TIMEOUT;
      uint16_t port{0};
      try {
        port = std::stoi(TCP);
      }
      catch (...) {
        port = 0;
      }
      auto connection = stringtoolbox::split(TCP, ':');

      const bool NOT_IS_CLIENT{(TCP.find(":") == std::string::npos) && (("1" == TCP) || (1024 > port))};
      const bool NOT_IS_SERVER{(TCP.find(":") != std::string::npos) && (2 != connection.size())};
      if ( NOT_IS_CLIENT || NOT_IS_SERVER ) {
        usage();
      }

      const bool IS_CLIENT{!NOT_IS_SERVER && (2 == connection.size())};
      const bool IS_SERVER{!NOT_IS_CLIENT && (1023 < port)};
      const bool IS_SERVER_TO_CLIENTS{(IS_SERVER && (1 == commandlineArguments.count("cid-from"))) || (IS_CLIENT && (1 == commandlineArguments.count("cid-to")))};

      if (IS_SERVER_TO_CLIENTS) {
        if (IS_CLIENT && !IS_SERVER) {
          try {
            port = std::stoi(connection[1]);

            cluon::TCPConnection c(connection[0], port);
            if (c.isRunning()) {
              cluon::UDPSender od4Destination{"225.0.0." + commandlineArguments["cid-to"], 12175};

              c.setOnNewData([&od4Destination](std::string &&d, std::chrono::system_clock::time_point && /*timestamp*/) {
                // Unpack multiple Envelopes.
                std::stringstream sstr(std::move(d));
                while (sstr.good()) {
                  auto retVal = cluon::extractEnvelope(sstr);
                  if (retVal.first) {
                    od4Destination.send(cluon::serializeEnvelope(std::move(retVal.second)));
                  }
                }
              });

              using namespace std::literals::chrono_literals;
              while (c.isRunning()) {
                std::this_thread::sleep_for(1s);
              }
            }
          }
          catch (...) {
            port = 0;
          }
        }
        else if (!IS_CLIENT && IS_SERVER) {
          std::vector<std::shared_ptr<cluon::TCPConnection>> connections;

          auto newConnectionHandler = [&argv, &connections](std::string &&from, std::shared_ptr<cluon::TCPConnection> conn) noexcept {
            std::cout << argv[0] << ": new connection from " << from << std::endl;
            conn->setOnNewData([](std::string &&/*d*/, std::chrono::system_clock::time_point && /*timestamp*/) {});
            conn->setOnConnectionLost([]() {});
            connections.push_back(conn);
          };
          cluon::TCPServer server(port, newConnectionHandler);

          std::mutex bufferForEnvelopesMutex;
          std::vector<char> bufferForEnvelopes;
          bufferForEnvelopes.reserve(65535);
          uint16_t indexBufferForEnvelopes{0};
          auto bufferOrSendEnvelope = [MTU, &connections, &bufferForEnvelopesMutex, &bufferForEnvelopes, &indexBufferForEnvelopes](cluon::data::Envelope &&env){
            const std::string serializedEnvelope{cluon::serializeEnvelope(std::move(env))};
            const auto LENGTH{serializedEnvelope.size()};

            std::lock_guard<std::mutex> lck(bufferForEnvelopesMutex);
            // Do we have to clear the buffer first?
            if ( (0 < indexBufferForEnvelopes) && (MTU < (indexBufferForEnvelopes + LENGTH)) ) {
              for(auto c: connections) {
                c->send(std::string(bufferForEnvelopes.data(), indexBufferForEnvelopes));
              }
              indexBufferForEnvelopes = 0;
            }

            std::memcpy(bufferForEnvelopes.data() + indexBufferForEnvelopes, serializedEnvelope.data(), LENGTH);
            indexBufferForEnvelopes += LENGTH;
            // Do we have to clear the buffer again?
            if (MTU < indexBufferForEnvelopes) {
              for(auto c: connections) {
                c->send(std::string(bufferForEnvelopes.data(), indexBufferForEnvelopes));
              }
              indexBufferForEnvelopes = 0;
            }
          };

          cluon::OD4Session od4Source(static_cast<uint16_t>(std::stoi(commandlineArguments["cid-from"])),
              [&connections, &bufferOrSendEnvelope, &mapOfEnvelopesToKeep, &mapOfEnvelopesToDrop, &downsampling, &downsamplingCounter](cluon::data::Envelope &&env) {
                if (!connections.empty()) {
                  auto id{env.dataType()};
                  if (0 < id) {
                    if ( downsampling.empty() && mapOfEnvelopesToKeep.empty() && mapOfEnvelopesToDrop.empty() ) {
                      bufferOrSendEnvelope(std::move(env));
                    } else if ( (0 < downsampling.size()) && downsampling.count(env.dataType()) ) {
                      downsamplingCounter[id] = downsamplingCounter[id] - 1;
                      if (downsamplingCounter[id] == 0) {
                        // Reset counter and forward Envelope.
                        downsamplingCounter[id] = downsampling[id];
                        bufferOrSendEnvelope(std::move(env));
                      }
                    } else {
                      if ((0 < mapOfEnvelopesToKeep.size()) && mapOfEnvelopesToKeep.count(id)) {
                        bufferOrSendEnvelope(std::move(env));
                      }
                      if ((0 < mapOfEnvelopesToDrop.size()) && !mapOfEnvelopesToDrop.count(id)) {
                        bufferOrSendEnvelope(std::move(env));
                      }
                    }
                  }
                }
              });

          const float FREQ{1000.0f/TIMEOUT};
          od4Source.timeTrigger(FREQ, [&od4Source, MTU, &connections, &bufferForEnvelopesMutex, &bufferForEnvelopes, &indexBufferForEnvelopes]() {
            std::lock_guard<std::mutex> lck(bufferForEnvelopesMutex);
            if (0 < indexBufferForEnvelopes) {
              for(auto c: connections) {
                c->send(std::string(bufferForEnvelopes.data(), indexBufferForEnvelopes));
              }
              indexBufferForEnvelopes = 0;
            }
            return od4Source.isRunning();
          });

          // Clear buffer for the last time.
          {
            std::lock_guard<std::mutex> lck(bufferForEnvelopesMutex);
            if (0 < indexBufferForEnvelopes) {
              for(auto c: connections) {
                c->send(std::string(bufferForEnvelopes.data(), indexBufferForEnvelopes));
              }
              indexBufferForEnvelopes = 0;
            }
          }

          connections.clear();
        }
        else {
          retCode = 1;
        }
      }
      else {
        if (IS_CLIENT && !IS_SERVER) {
          try {
            port = std::stoi(connection[1]);

            cluon::TCPConnection c(connection[0], port);
            if (c.isRunning()) {

              std::mutex bufferForEnvelopesMutex;
              std::vector<char> bufferForEnvelopes;
              bufferForEnvelopes.reserve(65535);
              uint16_t indexBufferForEnvelopes{0};
              auto bufferOrSendEnvelope = [MTU, &c, &bufferForEnvelopesMutex, &bufferForEnvelopes, &indexBufferForEnvelopes](cluon::data::Envelope &&env){
                const std::string serializedEnvelope{cluon::serializeEnvelope(std::move(env))};
                const auto LENGTH{serializedEnvelope.size()};

                std::lock_guard<std::mutex> lck(bufferForEnvelopesMutex);
                // Do we have to clear the buffer first?
                if ( (0 < indexBufferForEnvelopes) && (MTU < (indexBufferForEnvelopes + LENGTH)) ) {
                  c.send(std::string(bufferForEnvelopes.data(), indexBufferForEnvelopes));
                  indexBufferForEnvelopes = 0;
                }

                std::memcpy(bufferForEnvelopes.data() + indexBufferForEnvelopes, serializedEnvelope.data(), LENGTH);
                indexBufferForEnvelopes += LENGTH;
                // Do we have to clear the buffer again?
                if (MTU < indexBufferForEnvelopes) {
                  c.send(std::string(bufferForEnvelopes.data(), indexBufferForEnvelopes));
                  indexBufferForEnvelopes = 0;
                }
              };

              cluon::OD4Session od4Source(static_cast<uint16_t>(std::stoi(commandlineArguments["cid-from"])),
                  [&c, &bufferOrSendEnvelope, &mapOfEnvelopesToKeep, &mapOfEnvelopesToDrop, &downsampling, &downsamplingCounter](cluon::data::Envelope &&env){
                    auto id{env.dataType()};
                    if (0 < id) {
                      if ( downsampling.empty() && mapOfEnvelopesToKeep.empty() && mapOfEnvelopesToDrop.empty() ) {
                        bufferOrSendEnvelope(std::move(env));
                      }
                      else if ( (0 < downsampling.size()) && downsampling.count(env.dataType()) ) {
                        downsamplingCounter[id] = downsamplingCounter[id] - 1;
                        if (downsamplingCounter[id] == 0) {
                          // Reset counter and forward Envelope.
                          downsamplingCounter[id] = downsampling[id];
                          bufferOrSendEnvelope(std::move(env));
                        }
                      }
                      else {
                        if ( (0 < mapOfEnvelopesToKeep.size()) && mapOfEnvelopesToKeep.count(id) ) {
                          bufferOrSendEnvelope(std::move(env));
                        }
                        if ( (0 < mapOfEnvelopesToDrop.size()) && !mapOfEnvelopesToDrop.count(id) ) {
                          bufferOrSendEnvelope(std::move(env));
                        }
                      }
                    }
                  });

              const float FREQ{1000.0f/TIMEOUT};
              od4Source.timeTrigger(FREQ, [&od4Source, MTU, &c, &bufferForEnvelopesMutex, &bufferForEnvelopes, &indexBufferForEnvelopes](){
                std::lock_guard<std::mutex> lck(bufferForEnvelopesMutex);
                if (0 < indexBufferForEnvelopes) {
                  c.send(std::string(bufferForEnvelopes.data(), indexBufferForEnvelopes));
                  indexBufferForEnvelopes = 0;
                }
                return od4Source.isRunning();
              });

              // Clear buffer for the last time.
              {
                std::lock_guard<std::mutex> lck(bufferForEnvelopesMutex);
                if (0 < indexBufferForEnvelopes) {
                  c.send(std::string(bufferForEnvelopes.data(), indexBufferForEnvelopes));
                  indexBufferForEnvelopes = 0;
                }
              }
            }
          }
          catch (...) {
            port = 0;
          }
        }
        else if (!IS_CLIENT && IS_SERVER) {
          cluon::UDPSender od4Destination{"225.0.0." + commandlineArguments["cid-to"], 12175};
          std::vector<std::shared_ptr<cluon::TCPConnection>> connections;

          auto newConnectionHandler = [&argv, &connections, &od4Destination](std::string &&from, std::shared_ptr<cluon::TCPConnection> conn) noexcept {
            std::cout << argv[0] << ": new connection from " << from << std::endl;
            conn->setOnNewData([&od4Destination](std::string &&d, std::chrono::system_clock::time_point && /*timestamp*/) {
              // Unpack multiple Envelopes.
              std::stringstream sstr(std::move(d));
              while (sstr.good()) {
                auto retVal = cluon::extractEnvelope(sstr);
                if (retVal.first) {
                  od4Destination.send(cluon::serializeEnvelope(std::move(retVal.second)));
                }
              }
            });
            conn->setOnConnectionLost([]() {});
            connections.push_back(conn);
          };

          cluon::TCPServer server(port, newConnectionHandler);


          using namespace std::literals::chrono_literals;
          while (server.isRunning()) {
            std::this_thread::sleep_for(1s);
          }
          connections.clear();
        }
        else {
          retCode = 1;
        }
      }
    }
    else {
      cluon::UDPSender od4Destination{"225.0.0." + commandlineArguments["cid-to"], 12175};

      cluon::OD4Session od4Source(static_cast<uint16_t>(std::stoi(commandlineArguments["cid-from"])),
          [&od4Destination, &mapOfEnvelopesToKeep, &mapOfEnvelopesToDrop, &downsampling, &downsamplingCounter](cluon::data::Envelope &&env){
            auto id{env.dataType()};
            if (0 < id) {
              if ( downsampling.empty() && mapOfEnvelopesToKeep.empty() && mapOfEnvelopesToDrop.empty() ) {
                od4Destination.send(cluon::serializeEnvelope(std::move(env)));
              }
              else if ( (0 < downsampling.size()) && downsampling.count(env.dataType()) ) {
                downsamplingCounter[id] = downsamplingCounter[id] - 1;
                if (downsamplingCounter[id] == 0) {
                  // Reset counter and forward Envelope.
                  downsamplingCounter[id] = downsampling[id];
                  od4Destination.send(cluon::serializeEnvelope(std::move(env)));
                }
              }
              else {
                if ( (0 < mapOfEnvelopesToKeep.size()) && mapOfEnvelopesToKeep.count(id) ) {
                  od4Destination.send(cluon::serializeEnvelope(std::move(env)));
                }
                if ( (0 < mapOfEnvelopesToDrop.size()) && !mapOfEnvelopesToDrop.count(id) ) {
                  od4Destination.send(cluon::serializeEnvelope(std::move(env)));
                }
              }
            }
          });

      using namespace std::literals::chrono_literals;
      while (od4Source.isRunning()) {
        std::this_thread::sleep_for(1s);
      }
    }
  }
  return retCode;
}

