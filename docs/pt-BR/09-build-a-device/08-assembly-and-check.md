---
title: "Montagem do gabinete aquecido e verificação antes do lançamento"
description: "Montagem final do gabinete caseiro em ESP32: instalação no gabinete, primeiro aquecimento, calibração de temperatura e lista de verificação de segurança antes do funcionamento permanente."
---

# Montagem e verificação

Nesta página você monta o dispositivo no gabinete, faz o primeiro aquecimento controlado e verifica que o gabinete funciona com segurança. Faça as verificações em ordem e não deixe o dispositivo sem supervisão no primeiro lançamento.

## Ordem de montagem

1. Fixe o ESP32 e a parte de potência no gabinete de forma que as zonas de sinal fraco e de potência estejam separadas.
2. Coloque o sensor SHT31 no gabinete longe do fluxo direto do aquecedor — senão ele mostrará a temperatura do jato, não do ar no volume.
3. Fixe o termistor em contato térmico com o aquecedor.
4. Verifique que os fios não tocam o aquecedor e não caem no ventilador.
5. Na versão B (`220V`) certifique-se que os fios de rede são fixos nos terminais, a isolação está intacta, o gabinete é aterrado.

Requisitos para o gabinete e colocação dos nós — [Projeto de gabinete](../07-3d-printing/05-enclosure-design.md).

!!! warning "Peças plásticas perto do aquecimento"
    PLA amolece na temperatura que facilmente se encontra perto do aquecedor. Peças perto do calor imprima em material termoresistente: ABS, ASA ou PA (náilon). Veja [Materiais termoresistentes](../07-3d-printing/04-heat-resistant-materials.md) e [Por que PLA é uma escolha arriscada](../07-3d-printing/06-why-pla-is-risky.md).

## Verificação antes de energizar

Teste com multímetro antes do primeiro lançamento:

- sem curtos entre alimentação e terra;
- alimentação de sensores `3.3V`, não `5V`;
- terra comum do controlador e bloco de potência;
- termistor e resistor divisor montados corretamente;
- na versão B — aterramento do gabinete e fusível em lugar.

Como usar multímetro — [Multímetro](../05-tools/02-multimeter.md).

## Primeiro lançamento

1. Forneça energia apenas ao controlador e sensores. Se a fonte de alimentação for uma só para tudo, alimente o controlador separadamente: por USB ou através de um conversor abaixador, e não conecte ainda o fio do aquecedor.
2. Certifique-se que o dispositivo está Online no portal e mostra temperatura e umidade.
3. Conecte o aquecedor e ventilador.
4. Inicie o modo de manutenção de calor no portal e observe.

!!! danger "Não deixe o primeiro aquecimento sem supervisão"
    No primeiro lançamento, supervise o dispositivo. Certifique-se que o aquecedor desliga ao atingir a meta e pela proteção do termistor, não aquece continuamente.

O que observar nos primeiros minutos:

- temperatura do ar sobe e estabiliza perto da meta;
- temperatura do aquecedor não excede o limite definido;
- o aquecimento desliga ao atingir a meta e liga novamente após esfriar na quantidade de histerese;
- o ventilador funciona e não pega nos fios;
- o controlador não se reinicializa quando a carga liga.

## Calibração

Após o primeiro aquecimento, compare as leituras com um termômetro separado no gabinete:

- se a temperatura do ar no gabinete difere da meta — verifique a colocação do SHT31 (não deve estar no jato ou na parede);
- se a temperatura do aquecedor parece improvável — verifique o tipo de termistor e valor do resistor divisor;
- se necessário, corrija a temperatura-alvo e histerese no [menu](06-menu.md).

## Se algo não funciona

| Sintoma | Onde procurar |
|---------|---------------|
| Controlador se reinicializa na carga | [Erros de potência](../08-common-mistakes/02-power-mistakes.md) |
| Sensor mostra bobagem | [Erros de fiação](../08-common-mistakes/03-wiring-mistakes.md), [Verificação de termistor](../06-practical-guides/02-checking-thermistor.md) |
| Dispositivo não conecta ao Wi-Fi | [Erros de controlador](../08-common-mistakes/04-controller-mistakes.md) |
| Aquecedor/SSR aquece muito | [Erros de aquecedor e SSR](../08-common-mistakes/05-heater-ssr-mistakes.md) |

Sequência geral de diagnóstico — [Lista de verificação de diagnóstico](../08-common-mistakes/06-diagnostic-checklist.md).

## Lista de verificação antes de funcionamento permanente

- [ ] Dispositivo mantém temperatura-alvo e não aquece continuamente.
- [ ] Proteção do aquecedor por termistor funciona.
- [ ] Fios não tocam o aquecedor e ventilador.
- [ ] Peças plásticas perto do calor são termoresistentes.
- [ ] Na versão B: gabinete é aterrado, fusível instalado, isolação intacta.
- [ ] Dados no portal correspondem à temperatura real no gabinete.

## Resultado

Você montou um gabinete aquecido de armazenamento de filamento em ESP32 e `idryer-core`: o dispositivo lê o clima e temperatura do aquecedor, mantém temperatura especificada, protege o aquecedor de superaquecimento e é controlado do portal. É uma base completa sobre a qual você pode construir seus próprios módulos de ecossistema.

Componentes posteriores — iluminação, balança, RFID — o núcleo também suporta; eles podem ser adicionados pelo mesmo esquema: sensor ou periférico → telemetria ou comando → exibição no portal.
