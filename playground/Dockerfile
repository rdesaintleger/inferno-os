from node:24-bookworm as stage_cache

### refresh the 'apt' cache and download debian packages
run --mount=type=tmpfs,target=/tmp \
    apt-get update

from node:24-bookworm as base_image

run --mount=type=tmpfs,target=/tmp \
    --mount=type=cache,from=stage_cache,sharing=locked,source=/var/cache/apt,target=/var/cache/apt \
    --mount=type=cache,from=stage_cache,sharing=locked,source=/var/lib/apt,target=/var/lib/apt \
    apt-get install -y libxkbfile-dev libsecret-1-dev git sudo curl && \
    apt-get autoremove --yes

# Create theia user
run adduser theia --shell /bin/bash && \
    chown -R theia:theia /home/theia && \
    chmod 755 /home/theia

# Create inferno user and directories with sudo configuration
# Application will be copied to /home/inferno
# Default workspace is located at /home/project
run adduser inferno --shell /bin/bash && \
    mkdir -p /etc/sudoers.d && \
    echo "inferno ALL=(ALL) NOPASSWD: ALL" > /etc/sudoers.d/010_inferno-nopasswd && \
    chmod 644 /etc/sudoers.d/010_inferno-nopasswd && \
    chmod 700 /etc/sudoers.d && \
    chmod g+rw /home && \
    mkdir -p /home/project && \
    chown -R inferno:inferno /home/inferno && \
    chown -R inferno:inferno /home/project

from base_image as build_theia_stage

run --mount=type=tmpfs,target=/tmp \
    --mount=type=cache,from=stage_cache,sharing=locked,source=/var/cache/apt,target=/var/cache/apt \
    --mount=type=cache,from=stage_cache,sharing=locked,source=/var/lib/apt,target=/var/lib/apt \
    --mount=target=/tmp/export,type=bind \
    apt-get install -y build-essential make gcc

env HOME=/home/theia
user theia
workdir /home/theia

run cd $HOME && \
    git init . && \
    git remote add origin https://github.com/eclipse-theia/theia-blueprint.git && \
    git fetch origin a1d84dd993b785678118afb90a3057b350d5a831 && \
    git reset --hard FETCH_HEAD

run yarn --pure-lockfile && \
    yarn build:extensions

run yarn download:plugins

run yarn browser build && \
    yarn

run yarn autoclean --init && \
    echo *.ts >> .yarnclean && \
    echo *.ts.map >> .yarnclean && \
    echo *.spec.* >> .yarnclean && \
    yarn autoclean --force && \
    yarn cache clean && \
    rm -rf .git applications/electron theia-extensions/launcher theia-extensions/updater node_modules

from base_image as multilib_image

run --mount=type=tmpfs,target=/tmp \
    --mount=type=cache,from=stage_cache,sharing=locked,source=/var/cache/apt,target=/var/cache/apt \
    --mount=type=cache,from=stage_cache,sharing=locked,source=/var/lib/apt,target=/var/lib/apt \
    dpkg --add-architecture i386 && \
    apt-get update && \
    apt-get install -y libc6-dev-i386 libx11-6:i386 libxext-dev:i386 gcc-multilib

from multilib_image as build_inferno_stage

env HOME=/home/inferno
env PATH="/home/project/inferno-os/Linux/386/bin:${PATH}"
user inferno

run --mount=type=tmpfs,target=/tmp \
    --mount=type=cache,from=stage_cache,sharing=locked,source=/var/cache/apt,target=/var/cache/apt \
    --mount=type=cache,from=stage_cache,sharing=locked,source=/var/lib/apt,target=/var/lib/apt \
    cd /home/project && \
    git clone https://github.com/rdesaintleger/inferno-os.git && \
    cd /home/project/inferno-os && \
    git submodule update --init --recursive && \
    echo > mkconfig ROOT=/home/project/inferno-os && \
    echo >>mkconfig TKSTYLE=std && \
    echo >>mkconfig SYSHOST=Linux && \
    echo >>mkconfig SYSTARG=Linux && \
    echo >>mkconfig OBJTYPE=386 && \
    echo >>mkconfig 'OBJDIR=$SYSTARG/$OBJTYPE' && \
    echo >>mkconfig '<$ROOT/mkfiles/mkhost-$SYSHOST' && \
    echo >>mkconfig '<$ROOT/mkfiles/mkfile-$SYSTARG-$OBJTYPE' && \
    /home/project/inferno-os/makemk.sh && \
    mk nuke && \
    mk install

from multilib_image

run --mount=type=tmpfs,target=/tmp \
    --mount=type=cache,from=stage_cache,sharing=locked,source=/var/cache/apt,target=/var/cache/apt \
    --mount=type=cache,from=stage_cache,sharing=locked,source=/var/lib/apt,target=/var/lib/apt \
    DEBIAN_FRONTEND=noninteractive apt-get install -y git openssh-client bash libsecret-1-0 && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y novnc websockify && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends openbox tint2 xdg-utils lxterminal hsetroot tigervnc-standalone-server supervisor && \
    DEBIAN_FRONTEND=noninteractive apt-get --no-install-recommends install -y build-essential libx11-dev libxkbfile-dev sudo && \
    apt-get autoremove --yes

run --mount=type=tmpfs,target=/tmp \
    --mount=type=cache,from=stage_cache,sharing=locked,source=/var/cache/apt,target=/var/cache/apt \
    --mount=type=cache,from=stage_cache,sharing=locked,source=/var/lib/apt,target=/var/lib/apt \
    --mount=target=/tmp/export,type=bind \
    echo 'hsetroot -solid "#123456" &' >> /etc/xdg/openbox/autostart && \
    ln -s vnc.html /usr/share/novnc/index.html && \
    cp /tmp/export/playground/supervisord.conf /etc/supervisord.conf && \
    cp /tmp/export/playground/menu.xml /etc/xdg/openbox/menu.xml && \
    su inferno -c "mkdir -p /home/inferno/.config/tint2" && \
    su inferno -c "cp /tmp/export/playground/tint2rc /home/inferno/.config/tint2 && \
        mkdir -p /home/inferno/.local/share/icons/dist && \
        mkdir -p /home/inferno/.local/share/applications"

env PATH="/home/project/inferno-os/Linux/386/bin:${PATH}"

copy --from=build_theia_stage --chown=theia:theia /home/theia /home/theia
copy --from=build_inferno_stage /home/project/inferno-os /home/project/inferno-os

# expose theia port and noVNC port
expose 3000
expose 6080

# Specify default shell for Theia and the Built-In plugins directory
env SHELL=/bin/bash
env THEIA_DEFAULT_PLUGINS=local-dir:/home/theia/plugins

# Use installed git instead of dugite
env USE_LOCAL_GIT=true

# launch supervisord
entrypoint ["/bin/bash", "-c", "/usr/bin/supervisord"]
