# Caligo — Registro de Decisões

Versão 0.1

## Como funciona

Toda decisão tem ID, status e consequência. `FIXO` só muda por RFC em `docs/rfcs/`. `PROVISÓRIO` muda com nota no histórico. O escopo define o produto; este documento preserva o raciocínio para que a decisão não seja reaberta por memória seletiva.

| ID | Status | Decisão | Motivo | Consequência |
|---|---|---|---|---|
| D-001 | FIXO | Nenhum sistema entra se não aparecer numa cena jogável. | O protótipo provou sistemas isolados e não produziu um jogo. | Toda release termina em algo visível e jogável. |
| D-002 | FIXO | Duas colunas: cidade densa e natureza estilizada. | É o campo onde a Caligo pode ser referência real. | Nenhum sistema entra sem servir a uma das duas colunas. |
| D-003 | FIXO | Núcleo em C++20; gameplay em C# sobre .NET 10 LTS. | C++ sustenta o desempenho; C# sustenta a iteração. | A fronteira é uma ABI C estável, com custo de hospedagem e marshaling. |
| D-004 | FIXO | Windows 10/11 x64 e Direct3D 11 FL11_0 na v1. | Um alvo só permite qualidade de produção. | A RHI nasce preparada para DX12, mas a v1 não entrega DX12. |
| D-005 | FIXO | Um executável com editor e runtime; a cena é a fonte única de verdade. | Elimina divergência entre autoria e jogo. | Play usa snapshot e descarta o estado ao parar. |
| D-006 | FIXO | Sem placeholder, stub ou código morto. | Código provisório vira fundação falsa. | O CI barra marcadores e o revisor barra implementação incompleta. |
| D-007 | FIXO | TDD; compilação e teste apenas no CI. | O ambiente local não é confiável para Windows. | Todo commit é validado no GitHub Actions antes de ser considerado pronto. |
| D-008 | FIXO | Todo sistema visual tem teste de imagem determinístico. | "Parece certo" não é critério. | WARP, 300 frames para sistemas temporais e imagem dourada versionada. |
| D-009 | FIXO | Sem comentários no código. | Nome e estrutura devem explicar; comentário apodrece. | O CI rejeita comentários em C++, CMake e Python. |
| D-010 | FIXO | Decisão fixa só muda por RFC escrito. | Impede reabertura por impulso no meio de uma release. | O RFC contém problema, opções, custo, impacto e decisão. |
| D-011 | FIXO | O nome é Caligo, sem sufixo. | Uma marca, um produto, sem versão no nome. | A empresa continua Kizuri Studio; a engine é Caligo. |
| D-012 | FIXO | Sem rede, VR, mobile, console ou macOS na v1. | Cada plataforma multiplica o custo de produção. | Nenhuma abstração é criada para plataformas fora do alvo. |
| D-013 | FIXO | Sem visual scripting. | O gameplay é C# e o editor não carrega duas linguagens. | Ferramentas de autoria geram C# ou dados, nunca grafos. |
| D-014 | FIXO | Sem engine genérica, marketplace ou plugins de terceiros. | A Caligo serve aos jogos do estúdio, não a um ecossistema. | Toda API é desenhada para o produto, não para adoção externa. |
| D-015 | FIXO | Sem ray tracing obrigatório e sem fotorrealismo. | O alvo é estilizado e o piso é FL11_0. | Técnicas de imagem priorizam estabilidade e custo, não física exata. |
| D-016 | FIXO | Stack da seção 5 do escopo, sem vcpkg. | Dependências fixas e reproduzíveis. | FetchContent com tag fixa; atualização de dependência é mudança deliberada. |
| D-017 | FIXO | Asset fonte em formato aberto; runtime lê apenas cozido. | Fonte diffável e pacote rápido e estável. | Nenhum parser de formato aberto roda no jogo. |
| D-018 | FIXO | Meta de 1080p60 numa GTX 1060 6 GB. | Define o orçamento real de cada sistema. | Regressão acima de 10% no orçamento quebra a release. |
| D-019 | PROVISÓRIO | Piso de hardware: FL11_0, 2 GB de VRAM, 8 GB de RAM. | Garante alcance sem sacrificar o alvo. | Pendente de confirmação da GPU de teste do estúdio. |
| D-020 | FIXO | UI de jogo separada do editor: markup, estilo em folha e binding por view-model. | UI de jogo é conteúdo; ImGui é ferramenta. | Dois sistemas, dois orçamentos, nenhuma dependência cruzada. |
| D-021 | FIXO | Limite rígido de linhas por arquivo, verificado no CI. | Arquivo monolítico é o defeito mais caro de corrigir. | Exceção só por allowlist com motivo; entrada obsoleta quebra o CI. |
| D-022 | PROVISÓRIO | Limite de 600 linhas em `.cpp` e 400 em `.h`, revisado após a R2. | O número precisa de código real para ser calibrado. | Funções até 80 linhas; tipos até 20 métodos públicos. |
| D-023 | FIXO | Código proprietário; repositório público apenas para o CI gratuito. | O limite de minutos do CI privado impede o fluxo de validação. | Todo direito é reservado; visibilidade pública não concede licença. |

## Histórico

- **0.1** — registro inicial das decisões D-001 a D-023.
