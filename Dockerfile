# Copyright (c) 2022 Perets Dmytro
# Author: Perets Dmytro <imperator999mcpe@gmail.com>
#
# Personal usage is allowed only if this comment was not changed or deleted.
# Commercial usage must be agreed with the author of this comment.


FROM archlinux:base-devel

RUN pacman -Sy

RUN useradd -mg users -G wheel -s /bin/bash webserver
RUN echo '%wheel ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

RUN mkdir -p /webserver/
COPY * /webserver/
RUN chown webserver:users -R /webserver/

RUN mkdir -p /srv/webserver/
RUN chown webserver:users -R /srv/webserver/

RUN mkdir -p /srv/certs/
RUN chown webserver:users -R /srv/certs/

WORKDIR /webserver/
RUN ls -alshp
USER webserver
RUN makepkg -sif --noconfirm

EXPOSE 80 443

WORKDIR /srv/certs/
RUN /bin/bash -c "echo -e \"CASUBJ=\"/C=UA/ST=Ukraine/L=Zakarpattia/O=imperzer0/CN=CAwebserver\";\n\
                            CRTSUBJ=\"/C=UA/ST=Ukraine/L=Zakarpattia/O=imperzer0/CN=CRTwebserver\";\n\
                            # Generate CA (Certificate Authority)\n\
                            openssl genrsa -out ca.key 2048;\n\
                            openssl req -new -x509 -days 365 -key ca.key -out ca.pem -subj \\\$CASUBJ;\n\
                            # Generate server certificate\n\
                            openssl genrsa -out key.pem 2048;\n\
                            openssl req -new -key key.pem -out csr.pem -subj \\\$CRTSUBJ;\n\
                            openssl x509 -req -days 365 -in csr.pem -CA ca.pem -CAkey ca.key -set_serial 01 -out cert.pem;\"\
                  > generator.bash; chmod +x generator.bash; bash generator.bash"
RUN ls -alshp

WORKDIR /srv/webserver/
RUN ls -alshp

USER root
RUN ["/bin/bash", "-c", "echo -e 'cd /srv/webserver/;\n/bin/webserver $@;' > /script.bash; chmod +x /script.bash"]

USER webserver
ENTRYPOINT ["/bin/bash", "/script.bash", "--tls", "/srv/certs/"]
