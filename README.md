# Caligo

Engine de mundo aberto estilizado da Kizuri Studio.

- Escopo: [docs/ESCOPO.md](docs/ESCOPO.md)
- Arquitetura: [docs/ARQUITETURA.md](docs/ARQUITETURA.md)
- Formatos: [docs/FORMATOS.md](docs/FORMATOS.md)
- Decisões: [docs/DECISOES.md](docs/DECISOES.md)
- UI de jogo: [docs/sistemas/UI.md](docs/sistemas/UI.md)

## Licença

Proprietário. Todos os direitos reservados. A visibilidade pública existe apenas para o uso do GitHub Actions; nenhuma licença é concedida. Consulte [LICENSE](LICENSE).

## Estado

R0 — fundação. Núcleo, plataforma, janela, input, log, tempo, jobs, testes e modo headless.

## Construir

Windows 10/11 x64, Visual Studio 2022, CMake 3.24+, Ninja e Python 3.13.

```powershell
python -m pip install clang-format==21.1.8
python tools/check_source.py
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
build/bin/Debug/caligo.exe --headless --frames 120
```

## Executar

`build/bin/Debug/caligo.exe` abre a janela. `--headless --frames 120` roda a verificação sem janela.
