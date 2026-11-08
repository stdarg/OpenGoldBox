# Sourced by launchers that stage files with rsync. Git for Windows has no
# rsync, so where it is missing this defines one that copies with cp, for the
# options the launchers use: -a, --delete and --exclude=NAME, with SOURCE/ and
# DESTINATION/ naming folders. An excluded name is matched only at the top of
# the folders, where the launchers' are.
if ! command -v rsync >/dev/null 2>&1; then
    rsync() (
        local delete=no
        local excluded=()
        while [[ $1 == -* ]]; do
            case "$1" in
                -a) ;;
                --delete) delete=yes ;;
                --exclude=*) excluded+=("${1#--exclude=}") ;;
                *) printf 'rsync_fallback.sh: unsupported option %s\n' "$1" >&2; return 2 ;;
            esac
            shift
        done
        local source="${1%/}" destination="${2%/}"
        is_excluded() {
            local name
            for name in "${excluded[@]}"; do
                [[ $1 == "$name" ]] && return 0
            done
            return 1
        }
        shopt -s dotglob nullglob
        mkdir -p "$destination"
        local entry
        local copy=(cp -a)
        if [[ $delete == yes ]]; then
            # A fresh copy: nothing the source lost stays behind.
            for entry in "$destination"/*; do
                is_excluded "${entry##*/}" || rm -rf "$entry"
            done
        else
            # Only what changed: the portraits are large.
            copy+=(-u)
        fi
        for entry in "$source"/*; do
            is_excluded "${entry##*/}" && continue
            "${copy[@]}" "$entry" "$destination/"
        done
    )
fi
