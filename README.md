# webserver
Lightweight c++ web server for archlinux (available in docker container)

## Installation

### From repository
```bash
git clone https://github.com/imperzer0/webserver.git
cd webserver
```

#### Archlinux
```bash
makepkg -sif
```

#### Docker
```bash
docker build -t webserver .
```

## Usage

### On ArchLinux
Open up terminal (in ```website``` directory) and run commands
```bash
sudo mkdir -p /srv/webserver/
cd /srv/webserver/
webserver --address "http://0.0.0.0:PORT"
```

### In Docker
Open up terminal (in ```website``` directory) and run commands
```bash
docker run -dp 80:80 webserver
```
