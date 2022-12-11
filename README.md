# webserver

Lightweight c++ web server for archlinux with ftp (available in docker container)

## Installation

#### Archlinux

```bash
makepkg -sif
```

#### Docker

```bash
docker build -t webserver .
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
docker run --network=host -d webserver
```
