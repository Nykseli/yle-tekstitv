#/bin/bash

OPTION_LIST="
-h
-t
--help
--text-only
--version
--config
--help-config
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
--no-sub-page
--default-colors
--show-time
"

# Is _filedir declared
FILE_DIR=`declare -f _filedir`

_option_completion() {
    local cur prev
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    # Try to find file path after the config option is found
    if [[ ${prev} == "--config" ]]; then
        # Use compgen building file finder if _filedir is not declared
        if [[ -z $FILE_DIR ]]; then
            COMPREPLY=($(compgen -f -- ${cur}))
        else
            _filedir
        fi
    else
        # Else just complete the option normally
        COMPREPLY=($(compgen -W "${OPTION_LIST}" -- ${cur}))
    fi
}

complete -F _option_completion tekstitv
