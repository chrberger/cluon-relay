/*
 * Copyright (C) 2019  Christian Berger
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
    auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
    if (  (0 == commandlineArguments.count("cid-from"))
       || (0 == commandlineArguments.count("cid-to"))
       || ( (1 == commandlineArguments.count("cid-from")) && (1 == commandlineArguments.count("cid-to")) 
          && (commandlineArguments["cid-from"] == commandlineArguments["cid-to"]) )
       || ( (1 == commandlineArguments.count("keep")) && (1 == commandlineArguments.count("drop")) )
       ) {
        std::cerr << argv[0] << " relays Envelopes from one CID to another CID." << std::endl;
        std::cerr << "Usage:   " << argv[0] << " --cid-from=<source CID> --cid-to=<destination> [--keep=<list of messageIDs to keep>] [--drop=<list of messageIDs to drop>]" << std::endl;
        std::cerr << "         --cid-from:  relay Envelopes originating from this CID" << std::endl;
        std::cerr << "         --cid-to:    relay Envelopes to this CID (must be different from source)" << std::endl;
        std::cerr << "         --keep:      list of Envelope IDs to keep; example: --keep=19,25" << std::endl;
        std::cerr << "         --drop:      list of Envelope IDs to drop; example: --drop=17,35" << std::endl;
        std::cerr << "                      --keep and --drop must not be used simultaneously." << std::endl;
        std::cerr << "                      Not matching Envelope IDs with --keep are dropped." << std::endl;
        std::cerr << "                      Not matching Envelope IDs with --drop are kept." << std::endl;
        std::cerr << "Example: " << argv[0] << " --cid-from=111 --cid-to=112" << std::endl;
        retCode = 1;
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

        cluon::UDPSender od4Destination{"225.0.0." + commandlineArguments["cid-to"], 12175};

        cluon::OD4Session od4Source(static_cast<uint16_t>(std::stoi(commandlineArguments["cid-from"])),
            [&od4Destination, &mapOfEnvelopesToKeep, &mapOfEnvelopesToDrop](cluon::data::Envelope &&env){
                if (0 < env.dataType()) {
                    if ( (0 < mapOfEnvelopesToKeep.size()) && mapOfEnvelopesToKeep.count(env.dataType())) {
                        od4Destination.send(cluon::serializeEnvelope(std::move(env)));
                    }
                    if ( (0 < mapOfEnvelopesToDrop.size()) && !mapOfEnvelopesToDrop.count(env.dataType())) {
                        od4Destination.send(cluon::serializeEnvelope(std::move(env)));
                    }
                }
            }
        );

        using namespace std::literals::chrono_literals;
        while (od4Source.isRunning()) {
            std::this_thread::sleep_for(1s);
        }
    }
    return retCode;
}

