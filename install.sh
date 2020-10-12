#! /usr/bin/env sh
#
# Automatically download and install the latest version of SushiVM from GitHub
#

# Default settings
VMDIR="$HOME/.sushi/vm"
BINDIR="$HOME/.sushi/bin"
VERSION="latest"
UNAME="$(uname)"
# Operating system detection
SYSTEM=""
if [ "$UNAME" = "Linux" ]; then
	SYSTEM="linux"
elif [ "$UNAME" = "Darwin" ]; then
	SYSTEM="macos"
else
	echo "Unsupported system uname: $UNAME"
	exit 1
fi
echo "Operating system: $SYSTEM"
# Current version detection
INSTALLEDVM="$(which sushi 2> /dev/null)"
if [ -x "$INSTALLEDVM" ]; then
	INSTALLEDVERSION="$("$INSTALLEDVM" -v)"
fi
if [ "$INSTALLEDVERSION" != "" ]; then
	echo "Installed Sushi version: $INSTALLEDVERSION ($INSTALLEDVM)"
fi
# Latest version detection
LATESTURL="$(curl -s https://api.github.com/repos/eqela/sushivm/releases/${VERSION}|grep browser_download_url|grep "_${SYSTEM}.zip" | sed -e 's/.*"\(https:\/\/[^"]*\).*/\1/')"
LATESTVERSION="$(echo "$LATESTURL" | sed -e 's/.*\/sushi_\([^_]*\).*/\1/')"
if [ "$LATESTVERSION" = "" ]; then
	echo "ERROR: Failed to determine latest version"
	exit 1
fi
echo "Latest version: $LATESTVERSION ($LATESTURL)"
# Bail out if we are already on the latest version
if [ "$INSTALLEDVERSION" = "$LATESTVERSION" ]; then
	echo "Already using the latest version: $INSTALLEDVERSION"
	echo && "$INSTALLEDVM" || exit 1
	exit 0
fi
# Download the installer
FILENAME="$(basename "$LATESTURL")"
DOWNLOADDIR="$VMDIR/download"
DOWNLOADEDFILE="$DOWNLOADDIR/$FILENAME"
if [ ! -f "$DOWNLOADEDFILE" ]; then
	echo "Downloading: $LATESTURL"
	mkdir -p "$DOWNLOADDIR"
	curl -L -o "$DOWNLOADEDFILE" "$LATESTURL" || exit 1
fi
# Install the downloaded file
TARGETDIR="$VMDIR/$LATESTVERSION"
rm -rf "$TARGETDIR"
unzip "$DOWNLOADEDFILE" -d "$TARGETDIR" || exit 1
rm -f "$BINDIR/sushi"
mkdir -p "$BINDIR"
ln -s "../vm/$LATESTVERSION/sushi" "$BINDIR/sushi"
echo && "$BINDIR/sushi" || exit 1
# Update PATH
PATHDEF="PATH=\"$HOME/.sushi/bin:\$PATH\""
RCFILE=""
if [ "$SYSTEM" = "macos" ]; then
	RCFILE="$HOME/.zshrc"
else
	RCFILE="$HOME/.bashrc"
fi
if [ "$(grep "$PATHDEF" "$RCFILE")" = "" ]; then
	echo "$PATHDEF" >> "$RCFILE"
	echo "Your PATH definition in $RCFILE was modified. Restart your terminal for the new setting to take effect."
	echo
fi
exit 0
