---
title: "Montagem de armário aquecido e verificação antes de arranque"
description: "Montagem final de armário caseiro em ESP32: instalação na carcaça, primeiro aquecimento, calibração de temperatura e lista de verificação de segurança antes de funcionamento contínuo."
---

# Montagem e verificação

Nesta página você monta o dispositivo na carcaça, realiza o primeiro aquecimento controlado e verifica que o armário funciona de forma segura. Faça as verificações por ordem e não deixe o dispositivo sem vigilância na primeira ligação.

## Ordem de montagem

1. Prenda o ESP32 e a secção de potência na carcaça de modo que as zonas de baixa e alta potência estejam separadas.
2. Coloque o sensor SHT31 no armário longe do fluxo directo do aquecedor - caso contrário mostrará a temperatura do jato, não do ar no volume.
3. Prenda o termistor em contacto térmico com o aquecedor.
4. Verifique que os fios não tocam o aquecedor e não caem na ventoinha.
5. Na versão B (`220V`) certifique-se de que os fios de rede estão presos nas clemas, o isolamento está intacto e a carcaça está aterrada.

Requisitos para a carcaça e colocação de componentes - [Projecto de carcaça](../07-3d-printing/05-enclosure-design.md).

!!! warning "Peças impressas perto de calor"
    PLA amolece numa temperatura que se encontra facilmente perto do aquecedor. Peças perto de calor imprima com material resistente ao calor: ABS, ASA ou PA (nylon). Veja [Materiais à prova de calor](../07-3d-printing/04-heat-resistant-materials.md) e [Por que PLA é arriscado](../07-3d-printing/06-why-pla-is-risky.md).

## Verificação antes de fornecer alimentação

Teste com multímetro antes da primeira ligação:

- sem curtos-circuitos entre alimentação e terra;
- alimentação de sensores `3,3V`, não `5V`;
- terra comum do controlador e bloco de potência;
- termistor e resistor divisor montados correctamente;
- na versão B - aterramento de carcaça e fusível no lugar.

Como usar multímetro - [Multímetro](../05-tools/02-multimeter.md).

## Primeiro arranque

1. Forneça alimentação apenas ao controlador e sensores. Se a fonte de alimentação for uma só para tudo, alimente o controlador à parte: por USB ou através de um conversor step-down, e não ligue por enquanto o fio do aquecedor.
2. Certifique-se de que o dispositivo está Online no portal e mostra temperatura e humidade.
3. Ligue o aquecedor e a ventoinha.
4. Arranque o modo de manutenção de calor do portal e observe.

!!! danger "Não deixe o primeiro aquecimento sem vigilância"
    Na primeira ligação observe o dispositivo. Certifique-se de que o aquecedor desliga quando alcança o alvo e por protecção de termistor, não aquece continuamente.

O que observar nos primeiros minutos:

- a temperatura do ar sobe e estabiliza perto do alvo;
- a temperatura do aquecedor não excede o limite estabelecido;
- o aquecimento desliga ao alcançar o alvo e volta a ligar após arrefecimento pela quantidade de histerese;
- a ventoinha funciona e não toca nos fios;
- o controlador não reinicia quando a carga é ligada.

## Calibração

Após o primeiro aquecimento compare as leituras com um termómetro separado no armário:

- se a temperatura do ar no armário difere do alvo - verifique colocação de SHT31 (não deve estar num jato ou junto à parede);
- se a temperatura do aquecedor parece implausível - verifique tipo de termistor e valor nominal do resistor divisor;
- se necessário ajuste a temperatura-alvo e histerese no [menu](06-menu.md).

## Se algo não funciona

| Sintoma | Onde procurar |
|---------|---------------|
| O controlador reinicia sob carga | [Erros de potência](../08-common-mistakes/02-power-mistakes.md) |
| O sensor mostra disparates | [Erros de fiação](../08-common-mistakes/03-wiring-mistakes.md), [Verificação de termistor](../06-practical-guides/02-checking-thermistor.md) |
| O dispositivo não se liga ao Wi-Fi | [Erros de controladores](../08-common-mistakes/04-controller-mistakes.md) |
| Aquecedor/SSR aquece muito | [Erros de aquecedores e SSR](../08-common-mistakes/05-heater-ssr-mistakes.md) |

Sequência geral de diagnóstico - [Lista de verificação de diagnóstico](../08-common-mistakes/06-diagnostic-checklist.md).

## Lista de verificação antes de funcionamento contínuo

- [ ] O dispositivo mantém a temperatura-alvo e não aquece continuamente.
- [ ] A protecção do aquecedor por termistor funciona.
- [ ] Os fios não tocam o aquecedor e a ventoinha.
- [ ] Peças impressas perto de calor são resistentes ao calor.
- [ ] Na versão B: carcaça aterrada, fusível instalado, isolamento intacto.
- [ ] Dados no portal correspondem à temperatura real no armário.

## Resumo

Você montou um armário de armazenamento de filamento aquecido em ESP32 e `idryer-core`: o dispositivo lê clima e temperatura do aquecedor, mantém a temperatura estabelecida, protege o aquecedor contra sobreaquecimento e é controlado do portal. Esta é a base completa sobre a qual pode construir os seus próprios módulos do ecossistema.

Componentes adicionais - iluminação, balanças, RFID - o núcleo também suporta; podem ser adicionados pelo mesmo padrão: sensor ou periférico → telemetria ou comando → exibição no portal.
