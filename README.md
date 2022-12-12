# webserver

Lightweight c++ web server for archlinux with ftp (available in docker container)

## Installation

#### Archlinux

```bash
makepkg -sif
```

#### Docker

```bash
docker build --rm -t webserver .
```

## Usage

### ArchLinux

Open up terminal (in ```website``` directory) and run commands

```bash
sudo mkdir -p /srv/webserver/
cd /srv/webserver/
webserver
```

### Docker

Open up terminal (in ```website``` directory) and run commands

```bash
docker volume create webserver_certificates
docker volume create webserver_data
docker volume create webserver_etc
docker run --network=host -v webserver_certificates:/srv/certs \
           -v webserver_data:/srv/webserver \
           -v webserver_etc:/etc --name webserver -d webserver
```
