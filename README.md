# Simulador de Chat — Camada Física e de Enlace
### Teleinformática e Redes 1 — Universidade de Brasília - 2/2026

Esse projeto consiste em um simulador de chat com foco na implementação das camadas física e enlace da arquitetura em camadas de redes de computadores.

#### Stack
- C++20
- GTK (`gtkmm-4.0`)

### Como executar
O projeto foi desenvolvido para sistemas Linux e a única dependência é a biblioteca para criação de interfaces gráficas GTK, especificamente o pacote `gtkmm-4.0`, um _wrapper_ do GTK para C++.
Instruções para instalação da biblioteca podem ser consultadas em sua [página oficial](https://gnome.pages.gitlab.gnome.org/gtkmm-documentation/chapter-installation.html).\
Utilize o Makefile para compilar e linkar as dependências do projeto no executável do simulador (`tr1_sim`).
