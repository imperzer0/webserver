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
RUN chown webserver:users -R /webserver/
RUN mkdir -p /webserver-root/
RUN chown webserver:users -R /webserver-root/

COPY * /webserver/

WORKDIR /webserver/
RUN ls -alshp
USER webserver
RUN makepkg -sif --noconfirm

EXPOSE 80

WORKDIR /webserver-root/
RUN ls -alshp

ENTRYPOINT ["/bin/bash", "-c", "cd /webserver-root/; /bin/webserver"]