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
       ) {
        std::cerr << argv[0] << " relays Envelopes from one CID to another CID." << std::endl;
        std::cerr << "Usage:   " << argv[0] << " --cid-from=<source CID> --cid-to=<destination>" << std::endl;
        std::cerr << "         --cid-from:  relay Envelopes originating from this CID" << std::endl;
        std::cerr << "         --cid-to:    relay Envelopes to this CID (must be different from source)" << std::endl;
        std::cerr << "Example: " << argv[0] << " --cid-from=111 --cid-to=112" << std::endl;
        retCode = 1;
    } else {
        cluon::UDPSender od4Destination{"225.0.0." + commandlineArguments["cid-to"], 12175};

        cluon::OD4Session od4Source(static_cast<uint16_t>(std::stoi(commandlineArguments["cid-from"])),
            [&od4Destination](cluon::data::Envelope &&env){
                od4Destination.send(cluon::serializeEnvelope(std::move(env)));
            }
        );

        using namespace std::literals::chrono_literals;
        while (od4Source.isRunning()) {
            std::this_thread::sleep_for(1s);
        }
    }
    return retCode;
}

