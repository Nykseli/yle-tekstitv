#/bin/bash

OPTION_LIST="
-h
-t
--help
--text-only
--version
--bg-color
--text-color
--link-color
--navigation
--long-navigation
--no-nav
--no-top-nav
--no-bottom-nav
--no-title
--no-middle
"

complete -W "$OPTION_LIST" tekstitv
