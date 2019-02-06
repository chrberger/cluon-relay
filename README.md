## Microservice to relay Envelopes from one OD4Session to another one 

This repository provides source code for a microservice to relay Envelopes
from one OD4Session to another OD4Session.

[![License: GPLv3](https://img.shields.io/badge/license-GPL--3-blue.svg
)](https://www.gnu.org/licenses/gpl-3.0.txt)


## Table of Contents
* [Dependencies](#dependencies)
* [Usage](#usage)
* [Build from sources on the example of Ubuntu 16.04 LTS](#build-from-sources-on-the-example-of-ubuntu-1604-lts)
* [License](#license)


## Dependencies
You need a C++14-compliant compiler to compile this project.

The following dependency is part of the source distribution:
* [libcluon](https://github.com/chrberger/libcluon) - [![License: GPLv3](https://img.shields.io/badge/license-GPL--3-blue.svg
)](https://www.gnu.org/licenses/gpl-3.0.txt)


## Usage
To run this microservice using `docker-compose`, you can simply add the following
section to your `docker-compose.yml` to relay only envelopes `12` and `13` from
CID `109` to CID `111`:

```yml
version: '2' # Must be present exactly once at the beginning of the docker-compose.yml file
services:    # Must be present exactly once at the beginning of the docker-compose.yml file
    relay-envelopes:
        image: chrberger/cluon-relay-multi:v0.0.2
        restart: always
        network_mode: "host"
        command: "--cid-from=109 --cid-to=111 --keep=12,13"
```

Command for commandline to display the resulting image after operations:
```
docker run --rm -ti --init --net=host chrberger/cluon-relay-multi:v0.0.2 --cid-from=109 --cid-to=111 --keep=12,13
```

The parameters to the application are:
* `--cid-from`: relay Envelopes originating from this CID
* `--cid-to`: relay Envelopes to this CID (must be different from source)
* `--keep`: list of Envelope IDs to keep; example: --keep=19,25
* `--drop`: list of Envelope IDs to drop; example: --drop=17,35
* `--downsampling:` list of Envelope IDs to downsample; example: `--downsample=12:2,31:10`  keep every second of 12 and every tenth of 31


## Build from sources on the example of Ubuntu 16.04 LTS
To build this software, you need cmake, C++14 or newer, libyuv, libvpx, and make.
Having these preconditions, just run `cmake` and `make` as follows:

```
mkdir build && cd build
cmake -D CMAKE_BUILD_TYPE=Release ..
make && make test && make install
```


## License

* This project is released under the terms of the GNU GPLv3 License

