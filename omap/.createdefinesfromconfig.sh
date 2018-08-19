cat .config | grep -e "[^#].*=y$" | sed -e "s/\(.*\)=y/defined=\1/" >> .defines
