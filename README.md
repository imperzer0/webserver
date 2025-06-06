# webserver

Lightweight c++ web server for linux with ftp (available in docker container)

## Installation

#### Archlinux

Use [PKGBUILD](archpackage/PKGBUILD) to make and install the package

```bash
cd archpackage;
makepkg -sif
```

#### Debian

Use [makepkg.sh](debpackage/makepkg.sh) to build and install the package

```bash
cd debpackage;
bash makepkg.sh -i
```

#### Docker

```bash
docker build -t webserver . && docker image prune -f && docker compose up
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
echo -e "CASUBJ=\"/C=UA/ST=Ukraine/L=Zakarpattia/O=dima/CN=CAwebserver\";\n\
         CRTSUBJ=\"/C=UA/ST=Ukraine/L=Zakarpattia/O=dima/CN=CRTwebserver\";\n\
         # Generate CA (Certificate Authority)\n\
         openssl ecparam -genkey -name prime256v1 -out ca.key;\n\
         openssl req -new -x509 -days 365 -key ca.key -out ca.pem -subj \$CASUBJ;\n\
         # Generate server certificate\n\
         openssl ecparam -genkey -name prime256v1 -out key.pem;\n\
         openssl req -new -key key.pem -out csr.pem -subj \$CRTSUBJ;\n\
         # Sign the public key\n\
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
If you want to implement custom functionality - go to `config.cpp`.<br/>
Follow the guidelines in the comments to configure it properly.<br/>
Then you have to recompile and reinstall the application.

# Security tips

To protect your server from hackers:

1. Create a user for the webserver and use native linux protections to prevent users
   from accessing non-server files and directories
2. Avoid allowing users to create symlinks. They can escape their sandbox root directory
   and potentially get into other user's directory
3. Make sure that other services that have access to this server's directories won't
   execute or process in a way that could compromise the security of the machine
   files that users can create and modify
