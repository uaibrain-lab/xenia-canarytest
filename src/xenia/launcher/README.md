# launcheruwp integration (Xenia Canary)

Camada de integracao entre o launcher (`uaibrain-lab/launcheruwp`) e o motor do Xenia. O launcher e a interface; o Xenia roda por baixo como backend de execucao.

**Estado: nao compilado, nao testado.** Ligado por `-DXENIA_BUILD_LAUNCHER=ON` (desligado por padrao; builds normais nao mudam). Testes: `-DXENIA_BUILD_TESTS=ON` (alvo `xenia-launcher-tests`).

## Modos de execucao

- **emulated**: o arquivo e executado por um emulador (Xenia, Xbox 360).
- **recompiled**: jogo recompilado separado; o launcher so guarda arquivos e lanca por um adaptador registrado. Enquanto nenhum adaptador existir, o lancamento e bloqueado (nunca cai para emulacao em silencio).

## Portal de upload

O formulario do portal envia `mode` (obrigatorio: `emulated` ou `recompiled`), `target_id`, nome e tamanho. `game_upload_contract.h` valida tudo e define a pasta de destino, sempre separada por modo:

- `Games/emulated/<emulador>/<arquivo>`
- `Games/recompiled/<jogo>/<arquivo>`

As mesmas regras precisam rodar no servidor do portal, nunca so no navegador. Nao ha extracao, execucao nem ativacao no upload.

## Arquivos

- `execution_mode.h`: enum e parser estrito (sem valor padrao).
- `game_upload_contract.h`: regras do portal e destinos.
- `runtime_backend.h`: interface `RuntimeBackend`, `RecompiledBackend`, `LaunchRouter`.
- `xenia_backend.{h,cc}`: backend que chama `xe::Emulator::LaunchPath`.
- `testing/`: testes Catch2 dos contratos portateis.

## O que ainda falta

1. Host UWP do Xenia (janela CoreWindow, apresentacao D3D12, entrada por Windows.Gaming.Input, armazenamento em AppContainer). O Xenia hoje so tem backends Win32. `XeniaBackend` exige um `Emulator` com `Setup()` ja feito.
2. JIT no AppContainer: nao habilitar `codeGeneration` sem prova de que funciona no Xbox.
3. Servidor do portal LAN (pareamento, upload em streaming, SHA-256, promocao atomica) e a pagina com a escolha recompilado/emulado.
4. Tela do launcher (visual do launcheruwp) ligada ao `LaunchRouter`.
5. Outros emuladores: acrescentar uma entrada em `kEmulators` e um `RuntimeBackend`. Atencao a licencas: emuladores GPL (ex.: PCSX2/XBSX2) nao devem ser linkados no mesmo binario; manter como pacotes/processos separados.
