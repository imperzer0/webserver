# Copyright (c) 2022 Perets Dmytro
# Author: Perets Dmytro <dmytroperets@gmail.com>

### BUILD Container ###
FROM archlinux AS build

# Install core dependencies
RUN pacman -Sy base-devel --noconfirm --needed

# Create a non-root user and give him root access
RUN useradd -mg users -G wheel -s /bin/bash webserver
RUN echo 'webserver ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

# Create a build directory that our user has rights for
RUN mkdir -p /webserver/
COPY * /webserver/
RUN chown webserver:users -R /webserver/

# Build archlinux package
WORKDIR /webserver/
RUN ls -alshp
USER webserver
RUN makepkg -sif --noconfirm


### APP Container ###
FROM archlinux AS app

RUN mkdir -p /pack/
# Copy our packages from build environment
COPY --from=build /webserver/*.pkg.tar.zst /pack/
WORKDIR /pack/
# Install them all
RUN pacman -U *.pkg.tar.zst --noconfirm

# Non-root user
RUN [ "useradd", "-mG", "users", "-s", "/bin/bash", "webserver" ]

# Init Server root
RUN [ "mkdir", "-p", "/srv/webserver/" ]
RUN [ "chown", "-R", "webserver:webserver", "/srv/webserver/" ]
RUN [ "chmod", "-R", "644", "/srv/webserver/" ]
RUN [ "chmod", "744", "/srv/webserver/" ]

EXPOSE 80    443    21
#      http  https  ftp

# FTP Passive Mode Ports
EXPOSE 51480-52480

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
CMD ["--loglevel", "4"]
