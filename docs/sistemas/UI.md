# Caligo — UI de Jogo

Versão 0.1

## 1. Responsabilidade

O sistema de UI de jogo desenha, organiza, anima e traduz tudo que o jogador vê fora do mundo 3D: HUD, menus, diálogos, notificações, mapa, marcadores e prompts. Ele não é o editor. O editor usa ImGui; o jogo usa a UI da Caligo. São dois sistemas separados, com regras e orçamentos próprios.

## 2. Pipeline

```
markup (.cui) + estilo (.cuistyle) + fontes + sprites
        │
        ▼ importação e cook
descritor binário + atlas + SDF de fonte
        │
        ▼ runtime
documento → layout → draw list → passe de UI
        ▲
view-models em C# ← estado do jogo
```

## 3. Documento declarativo

A UI é declarada em texto, não montada em código. O markup é a fonte de verdade, versionável e diffável.

```xml
<document name="Hud">
  <canvas>
    <panel class="vitals" anchor="top-left" offset="32,32">
      <bar bind="Player.Health" class="bar-health"/>
      <bar bind="Player.Stamina" class="bar-stamina"/>
    </panel>

    <text class="money" anchor="top-right" offset="-32,32"
          bind="Player.Money"/>

    <minimap class="map" anchor="bottom-right" offset="-32,-32"
             size="240,240" rotation="player"/>

    <text class="prompt" anchor="center-bottom" offset="0,-120"
          bind="Interaction.Prompt"/>
  </canvas>
</document>
```

Nós disponíveis: `canvas`, `panel`, `stack`, `grid`, `scroll`, `text`, `image`, `button`, `slider`, `toggle`, `list`, `bar`, `minimap`, `world-anchor`, `spacer`.

Nenhuma lógica no markup. Só estrutura, estilo e binding.

## 4. Layout

- Unidade de layout: pixel independente de densidade (DIP).
- Posicionamento por âncora, pivô e deslocamento.
- Containers: pilha horizontal, pilha vertical, grade e scroll.
- Tamanho por conteúdo, por fração ou por valor fixo.
- Safe area respeitada: notch, barra de tarefas e área segura do monitor.
- Layout recalculado só quando sujo. Nada de reconstruir a árvore por frame.
- Escala global de UI independente da resolução.

## 5. Estilo e tema

Estilo em folha separada, com seletores por classe, id e estado (`normal`, `hover`, `pressed`, `disabled`, `selected`).

```css
.vitals { spacing: 8; }
.bar-health { height: 12; fill: #d94b4b; background: #1a1a1a; }
.money { font: "Inter/24"; color: #f2e6c9; }
```

- Tema trocável em runtime, sem recarregar documento.
- Nove fatias para painéis e botões.
- Sombra, borda, raio e gradiente no estilo, nunca no código.
- Nenhuma cor ou fonte fixa em widget.

## 6. Binding

- View-models em C# expõem propriedades e comandos.
- Binding por caminho (`Player.Health`), uma via por padrão, duas vias para entrada.
- Binding nulo é seguro: widget fica oculto, erro aparece no editor.
- Nenhum widget lê o mundo diretamente. Toda leitura passa por view-model.
- Atualização por frame só para o que mudou.

```csharp
public sealed class Hud : UiDocument
{
    public PlayerView Player { get; init; } = null!;
    public InteractionView Interaction { get; init; } = null!;
}
```

## 7. Input

- Árvore de foco, pilha de modais e navegação por controle.
- Ponteiro capturado por widget; clique fora fecha modal.
- A UI consome input antes do jogo. Uma camada pode ou não bloquear gameplay.
- Glifos de input mudam conforme o dispositivo, sem texto fixo.
- Repetição de tecla, aceleração de analógico e dead zone no sistema, não no widget.

## 8. Animação

- Tweens de posição, tamanho, opacidade, cor e recorte.
- Transições por estado, com curva e duração no estilo.
- Timeline para sequências (abertura de menu, missão concluída).
- Nenhuma alocação por frame durante animação.
- Animação de layout não quebra o layout: o estado final é sempre o calculado.

## 9. Render

- Draw list de quads e texto, agrupada por atlas e estado.
- Pilha de recorte e máscara.
- Passe próprio depois do pós-processamento, em sRGB.
- Profundidade desligada; ordem explícita por camada.
- Orçamento: menos de 1 ms de CPU, menos de 1 ms de GPU, menos de 40 draw calls em 1080p.

## 10. Texto e fontes

- Importação de TTF e OTF, atlas SDF gerado no cook.
- Atlas dinâmico para caracteres fora do conjunto inicial.
- Cadeia de fallback por idioma, nunca crash por glifo ausente.
- Métricas de linha, kerning e alinhamento no layout.
- Rich text limitado: cor, negrito e ícone inline.

## 11. Localização

- Toda string por chave, nunca literal no markup.
- Regras de plural por idioma.
- Troca de idioma em runtime.
- Fonte e direção prontas para RTL, mesmo que a v1 não traduza.
- Chave ausente: mostra a chave em desenvolvimento, fallback em produção.

## 12. UI de mundo

- Marcadores, nomes, números de dano e prompts ancorados no mundo.
- Projeção mundo → tela com recorte fora da tela e seta de borda.
- Oclusão por traço de raio em frequência baixa, nunca por frame.
- Empilhamento para evitar sobreposição.
- Pool fixo: 64 marcadores simultâneos.

## 13. Minimapa

- Câmera ortográfica própria, renderizando para um alvo de 256×256.
- Atualização a 20 Hz, não a 60.
- Camada de mapa simplificada (ruas e quarteirões), não a cena completa.
- Rotação com o jogador ou norte fixo, configurável.
- Ícones desenhados pela UI, com recorte circular e zoom.
- Descoberta e névoa de guerra ficam para depois da fatia.

## 14. Acessibilidade

- Escala de UI, tamanho de legenda e contraste.
- Paletas para daltonismo.
- Remapeamento completo de input.
- Alternância entre segurar e apertar.
- Legendas com identificação de locutor.
- Nada disso é opcional.

## 15. DPI e resolução

- Layout em DIP com fator de escala.
- Testado em 1280×720, 1920×1080, 2560×1440 e 3840×2160.
- Testado em 16:9, 21:9 e 4:3.
- Safe area do sistema aplicada antes do layout.

## 16. Editor visual

- Canvas com arraste, âncoras e guias.
- Hierarquia, inspetor, editor de estilo e pré-visualização ao vivo.
- Undo/redo integrado ao undo do editor.
- O editor altera o markup, não um formato paralelo.
- Pré-visualização com dados falsos e com dados reais do play.

## 17. Falhas

- Fonte ausente: fallback, log único.
- Sprite ausente: xadrez magenta, log único.
- Binding quebrado: widget oculto, erro visível no editor.
- Chave de localização ausente: chave em desenvolvimento, fallback em produção.
- Documento inválido: erro no cook, nunca no runtime.

## 18. Testes

- Imagem dourada de layout em quatro resoluções e três proporções.
- Teste de foco, modal e navegação por controle.
- Teste de binding nulo e de propriedade inexistente.
- Teste de fallback de fonte e de localização.
- Teste de orçamento: CPU e GPU abaixo do limite.

## 19. Fora de escopo

- UI em HTML, CSS ou tecnologia web.
- Framework de UI de terceiros no runtime.
- UI 3D diegética complexa (telas interativas no mundo) antes da fatia.
- Suporte a VR.
