# Copyright (c) 2022 Perets Dmytro
# Author: Perets Dmytro <imperator999mcpe@gmail.com>
#
# Personal usage is allowed only if this comment was not changed or deleted.
# Commercial usage must be agreed with the author of this comment.


FROM archlinux AS build

RUN pacman -Sy base-devel --noconfirm --needed

RUN useradd -mg users -G wheel -s /bin/bash webserver
RUN echo '%wheel ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

RUN mkdir -p /webserver/
COPY * /webserver/
RUN chown webserver:users -R /webserver/

WORKDIR /webserver/
RUN ls -alshp
USER webserver
RUN makepkg -sif --noconfirm

FROM archlinux

RUN mkdir -p /pack/
COPY --from=build /webserver/*.pkg.tar.zst /pack/
WORKDIR /pack/
RUN pacman -Sy vim nano --needed
RUN pacman -U *.pkg.tar.zst --noconfirm

RUN mkdir -p /srv/webserver/
RUN mkdir -p /srv/certs/

EXPOSE 80 443 21

WORKDIR /srv/certs/
RUN /bin/bash -c "[[ \$([ ! -e /srv/certs/ca.key ]) || \
                  \$([ ! -e /srv/certs/ca.pem ]) || \
                  \$([ ! -e /srv/certs/cert.pem ]) || \
                  \$([ ! -e /srv/certs/csr.pem ]) || \
                  \$([ ! -e /srv/certs/key.pem ]) ]] && echo \"All files present\" && \
                  echo -e \"CASUBJ=\"/C=UA/ST=Ukraine/L=Zakarpattia/O=imperzer0/CN=CAwebserver\";\n\
                            CRTSUBJ=\"/C=UA/ST=Ukraine/L=Zakarpattia/O=imperzer0/CN=CRTwebserver\";\n\
                            # Generate CA (Certificate Authority)\n\
                            openssl genrsa -out ca.key 2048;\n\
                            openssl req -new -x509 -days 365 -key ca.key -out ca.pem -subj \\\$CASUBJ;\n\
                            # Generate server certificate\n\
                            openssl genrsa -out key.pem 2048;\n\
                            openssl req -new -key key.pem -out csr.pem -subj \\\$CRTSUBJ;\n\
                            openssl x509 -req -days 365 -in csr.pem -CA ca.pem -CAkey ca.key -set_serial 01 -out cert.pem;\"\
                  > generator.bash && chmod +x generator.bash && bash generator.bash || exit 0"
RUN ls -alshp

WORKDIR /srv/webserver/
RUN ls -alshp

RUN ["/bin/bash", "-c", "echo -e 'cd /srv/webserver/;\n/bin/webserver $@;' > /script.bash; chmod +x /script.bash"]

#RUN pacman -Sy openssh gdb rsync --noconfirm --needed
#EXPOSE 2222
#RUN /usr/bin/ssh-keygen -A
#RUN echo -e "Port 2222\n\
#          PermitRootLogin yes\n\
#          PermitEmptyPasswords yes\n\
#          AuthorizedKeysFile      .ssh/authorized_keys\n\
#          KbdInteractiveAuthentication no\n\
#          UsePAM yes\n\
#          PrintMotd no\n\
#          Subsystem       sftp    /usr/lib/ssh/sftp-server" > /etc/ssh/sshd_config
#RUN echo -e "root:\$6\$MVBXiuW1Hhl3OeF9\$M01MasH4nnCwfypFNgIbniEKKe2yPi/hdqbggFd1.KkjYjqxP9Hr2J1i95Q1TSo4ySqewc62Xezi6LcBE3OEp.:19322:0:99999:7:::\n\
#    webserver:\$6\$MVBXiuW1Hhl3OeF9\$M01MasH4nnCwfypFNgIbniEKKe2yPi/hdqbggFd1.KkjYjqxP9Hr2J1i95Q1TSo4ySqewc62Xezi6LcBE3OEp.:19322:0:99999:7:::" > /etc/shadow
#RUN echo -e "root:\$6\$MVBXiuW1Hhl3OeF9\$M01MasH4nnCwfypFNgIbniEKKe2yPi/hdqbggFd1.KkjYjqxP9Hr2J1i95Q1TSo4ySqewc62Xezi6LcBE3OEp.:19322:0:99999:7:::\n\
#    webserver:\$6\$MVBXiuW1Hhl3OeF9\$M01MasH4nnCwfypFNgIbniEKKe2yPi/hdqbggFd1.KkjYjqxP9Hr2J1i95Q1TSo4ySqewc62Xezi6LcBE3OEp.:19322:0:99999:7:::" > /etc/shadow-

ENTRYPOINT ["/bin/bash", "/script.bash", "--tls", "/srv/certs/"]
#ENTRYPOINT ["/bin/sshd", "-D"]
