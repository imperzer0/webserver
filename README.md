# webserver

Lightweight c++ web server for linux with ftp (available in docker container)

## Installation

#### Archlinux

Use [PKGBUILD](PKGBUILD) to make and install the package

```bash
makepkg -sif
```

#### Debian

Use [makepkg.sh](debpkg%2Fmakepkg.sh) to build and install the package

```bash
bash makepkg.sh -i
```

#### Docker

```bash
docker build --rm -t webserver .
```

## Setup email

1. Create google account with new email.
2. Go to your new account's settings.
3. Go to `Security` tab.
4. Enable 2-step verification.
5. After that you should see entry called `App passwords`, that's what we need: go there.
6. Tap on `Select app` field.
7. Choose entry `Other (custom name)` from dropdown menu.
8. Call it somehow. For example: `smtp_webserver`.
9. Press button called `Generate`.
10. Here you should see the key that looks like this `wlgf hdfr jwsr ydrg`.
11. Copy it.
12. Then go to command prompt and enter [commands below](#Deploy), replacing `<account>` with your gmail account name
    and `<auth_key>` with the key, you just copied.

## Deploy

### Linux

First you need to generate ssl certificates (only once)

```bash
sudo mkdir -p /srv/certs/
cd /srv/certs/
echo -e "CASUBJ=\"/C=UA/ST=Ukraine/L=Zakarpattia/O=imperzer0/CN=CAwebserver\";\n\
         CRTSUBJ=\"/C=UA/ST=Ukraine/L=Zakarpattia/O=imperzer0/CN=CRTwebserver\";\n\
         # Generate CA (Certificate Authority)\n\
         openssl genrsa -out ca.key 2048;\n\
         openssl req -new -x509 -days 365 -key ca.key -out ca.pem -subj \$CASUBJ;\n\
         # Generate server certificate\n\
         openssl genrsa -out key.pem 2048;\n\
         openssl req -new -key key.pem -out csr.pem -subj \$CRTSUBJ;\n\
         openssl x509 -req -days 365 -in csr.pem -CA ca.pem -CAkey ca.key -set_serial 01 -out cert.pem;" \
  >generator.bash && chmod +x generator.bash && bash generator.bash
```

Then, in order to run the server, navigate to the website's home directory and run commands.
Replace `<account>@gmail.com` with your email address and `<auth_key>` with [the key](#Setup-email) you just generated.

```bash
sudo mkdir -p /srv/webserver/
cd /srv/webserver/
webserver --tls /srv/certs/ --email="<account>@gmail.com" --email-password="<auth_key>"
```

### Docker

Open up terminal (in `website` directory) and run commands

```bash
docker volume create webserver_certificates
docker volume create webserver_data
docker volume create webserver_etc
docker run --network=host -v webserver_certificates:/srv/certs \
           -v webserver_data:/srv/webserver \
           -v webserver_etc:/etc --name webserver -it webserver \
           --email="<account>@gmail.com" --email-password="<auth_key>"
```

## Modification

`webserver` is a flexible application.<br/>
If you want to implement custom functionality,<br/>
I recommend doing so in `config.cpp`.<br/>
Follow the guidelines in the comments to configure it properly.<br/>
I *don't recommend* touching any other files for it can make the app unstable<br/>
if you don't completely understand what they do, but you can definitely try.<br/>
