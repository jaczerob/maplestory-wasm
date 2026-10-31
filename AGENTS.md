# AGENTS.md

## Scope

This file tells agents how to build and run this repository without guessing.
Prefer the local workflow first. Use Docker when the local workflow is not possible or the required local toolchain is unavailable.

**IMPORTANT**
This is a client only project. You cannot change the server behavior in any way.
Assume the server is working as expected.
Any issues or new features should utilize server existing infra and modify the client only.

## Hard Rules

- Treat `assets-updated/wz/` as read-only source input.
- Use an explicit absolute asset root when starting the asset server or tests.
- The runtime asset server receives only `manifest.json`, `shared/`, and `client/`.
- Do not discover workspace sibling paths automatically. Docker mounts must name their asset root explicitly.
- Do not change build commands just to work around a missing local dependency. Use the documented Docker fallback instead.

## Repo Facts

- WASM client source: `src/client`
- Shared NX library: `src/nlnx`
- Client build output: `build/JourneyClient.js`, `build/JourneyClient.wasm`, and optionally `build/JourneyClient.wasm.map`
- Local web entrypoints: `web/server.py`, `web/ws_proxy.py`, `web/assets_server.py`
- Docker web stack: `../docker/frontend/compose.yml`
- The client is designed to run with Cosmic server

## Client Build

Preferred build command:

```bash
./scripts/build_wasm.sh
```

Useful variants:

```bash
./scripts/build_wasm.sh --debug # --debug is the same as -g
./scripts/build_wasm.sh --jobs 4
```

If the local Emscripten or CMake toolchain is unavailable, or the local build is not possible, use the Docker fallback:

```bash
../docker/frontend/scripts/docker_build_wasm.sh
```

The Docker fallback accepts the same flags:

```bash
../docker/frontend/scripts/docker_build_wasm.sh --debug
../docker/frontend/scripts/docker_build_wasm.sh --jobs 4
```

## Local Deployment

Use local deployment when the local toolchain and local services are available.

1. Build the client with `./scripts/build_wasm.sh`.
2. If that is not possible, build the client with `../docker/frontend/scripts/docker_build_wasm.sh`.
3. Install the local Python dependency if needed:

```bash
pip install -r web/requirements.txt
```

4. Start the web services from the repository root in separate terminals:

```bash
python3 web/server.py
python3 web/ws_proxy.py --ws-port 8080
python3 web/assets_server.py --port 8765 --directory /absolute/path/to/assets
```

5. Open `http://localhost:8000`.

Assume the websocket proxy is forwarding to a running Cosmic server unless the user explicitly says otherwise.

## Docker Deployment

Use Docker when local deployment is not practical.

Preferred Docker web-stack command:

```bash
../docker/frontend/scripts/run_all.sh
```

This script creates the shared Docker network and starts the root web containers.

For the web side only:

```bash
../docker/frontend/scripts/docker_web_up.sh -d
```

To stop everything:

```bash
../docker/frontend/scripts/stop_all.sh
```

## Agent Behavior

- Prefer `./scripts/build_wasm.sh` for client builds.
- Fall back to `../docker/frontend/scripts/docker_build_wasm.sh` only when the local build is not possible.
- Prefer local deployment first when the environment supports it.
- Fall back to Docker deployment when local deployment is blocked by missing dependencies or services.
- Leave `assets-updated/wz/` untouched.

## Coding Styles:

* Write generic code whenever possible.
* Never write unsafe code.
* Add meaningful comments
  * Don't just repeat the code in the comment, explain the reasoning behind it.
* Don't have any credit or whatever in the source files (like license, reminder of journey client) in any comment

## Task Completion
* Make sure the project builds.
* Build should have no warnings.

## Documentation
* Check `docs/ms-network-protocol.md` for the network protocol.
