# Copyright (c) 2022 Perets Dmytro
# Author: Perets Dmytro <dmytroperets@gmail.com>

### BUILD Container ###
FROM debian:bookworm AS build

# UPGRADE
RUN [ "apt-get", "update", "--yes" ]
RUN [ "apt-get", "upgrade", "--yes" ]
# We need sudo for makepkg.sh
RUN [ "apt-get", "install", "--yes", "sudo" ]

# And non-root user
RUN [ "useradd", "-G", "sudo", "-s", "/bin/bash", "build" ]
RUN echo '%sudo   ALL=(ALL:ALL) NOPASSWD:ALL' >> /etc/sudoers

# Build directory /webserver/
RUN [ "mkdir", "-p", "/webserver/" ]
COPY . /webserver/
RUN [ "chown", "-R", "build:build", "/webserver/" ]

# Run packaging script
WORKDIR /webserver/debpackage/
USER build
RUN [ "bash", "makepkg.sh" ]
RUN ls -lh *.deb
RUN ls -lh src/ftp/debpackage/*.deb


### APP Container ###
FROM debian:bookworm AS app

# UPGRADE
RUN [ "apt-get", "update", "--yes" ]
RUN [ "apt-get", "upgrade", "--yes" ]

# Non-root user
RUN [ "useradd", "-mG", "users", "-s", "/bin/bash", "webserver" ]

# Copy built packages from 'build' container
RUN [ "mkdir", "-p", "/pack/ftp/" ]
COPY --from=build /webserver/debpackage/src/ftp/debpackage/*.deb /pack/ftp/
COPY --from=build /webserver/debpackage/*.deb /pack/

# Install packages
WORKDIR /pack/
RUN dpkg --unpack ftp/*.deb
RUN dpkg --unpack *.deb
RUN [ "apt-get", "install", "--yes", "--fix-broken" ]

# Init Server root
RUN [ "mkdir", "-p", "/srv/webserver/" ]
RUN [ "chown", "-R", "webserver:webserver", "/srv/webserver/" ]
RUN [ "chmod", "-R", "644", "/srv/webserver/" ]
RUN [ "chmod", "744", "/srv/webserver/" ]

#      http  https  ftp
EXPOSE 80    443    21

# Create Certificates in /srv/certs/
RUN [ "mkdir", "-p", "/srv/certs/" ]
WORKDIR /srv/certs/
RUN [ "chmod", "-R", "700", "/srv/certs/" ]
RUN /bin/bash -c "echo -e \"CASUBJ=\"/C=UA/ST=Ukraine/L=Zakarpattia/O=dima/CN=CAwebserver\";\n\
                            CRTSUBJ=\"/C=UA/ST=Ukraine/L=Zakarpattia/O=dima/CN=CRTwebserver\";\n\
                            # Generate CA (Certificate Authority)\n\
                            openssl ecparam -genkey -name prime256v1 -out ca.key;\n\
                            openssl req -new -x509 -days 365 -key ca.key -out ca.pem -subj \\\$CASUBJ;\n\
                            # Generate server certificate\n\
                            openssl ecparam -genkey -name prime256v1 -out key.pem;\n\
                            openssl req -new -key key.pem -out csr.pem -subj \\\$CRTSUBJ;\n\
                            # Sign the public key\n\
                            openssl x509 -req -days 365 -in csr.pem -CA ca.pem -CAkey ca.key -set_serial 01 -out cert.pem;\"\
                  > generator.bash || exit 2;"
RUN [ "chmod", "+x", "generator.bash" ]
RUN [ "bash", "generator.bash" ]
RUN chmod -R 600 /srv/certs/*
RUN [ "chown", "-R", "webserver:webserver", "/srv/certs/" ]
# Check
RUN openssl x509 -in /srv/certs/cert.pem -text -noout && \
    openssl pkey -in /srv/certs/key.pem -check -noout

# Init Server config
RUN [ "mkdir", "-p", "/etc/webserver/" ]
RUN [ "chown", "-R", "webserver:webserver", "/etc/webserver/" ]
RUN [ "chmod", "-R", "700", "/etc/webserver/" ]

### RUN Configuration ###
USER webserver
WORKDIR /srv/webserver/

ENTRYPOINT ["/bin/webserver", "--tls", "/srv/certs/"]
CMD []
