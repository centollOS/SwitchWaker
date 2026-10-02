# Container helpers shared by the Switch build scripts. Source this file; it
# selects Podman (rootless, as before) or Docker. Set SWITCH_CONTAINER_ENGINE
# to podman or docker to force one.

container_engine() {
    if [[ -n ${SWITCH_CONTAINER_ENGINE:-} ]]; then
        printf '%s\n' "$SWITCH_CONTAINER_ENGINE"
    elif command -v podman >/dev/null 2>&1; then
        echo podman
    elif command -v docker >/dev/null 2>&1; then
        echo docker
    fi
}

container_image_exists() {
    local engine=$1 image=$2
    if [[ $engine == podman ]]; then
        podman image exists "$image"
    else
        docker image inspect "$image" >/dev/null 2>&1
    fi
}

# container_run ENGINE ROOT [run options...] IMAGE COMMAND...
# Mounts ROOT at /work and runs as the invoking user so build outputs stay
# owned by them.
container_run() {
    local engine=$1 root=$2
    shift 2
    if [[ $engine == podman ]]; then
        podman run --rm --userns=keep-id -v "$root:/work:Z" -w /work "$@"
    else
        docker run --rm --user "$(id -u):$(id -g)" -e HOME=/tmp \
            -v "$root:/work" -w /work "$@"
    fi
}
