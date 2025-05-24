---
applyTo: '**'
---
Coding standards, domain knowledge, and preferences that AI should follow.Guia de Padrões e Diretrizes de Desenvolvimento

Utilize este guia como referência obrigatória ao escrever código C++ para este projeto. O objetivo é garantir legibilidade, manutenibilidade, eficiência e arquitetura sólida.


---

1. Boas Práticas de Programação

Legibilidade em primeiro lugar: Código é lido mais vezes do que é escrito. Escreva de forma clara, objetiva e organizada.

Nomes significativos: Variáveis, funções, classes e arquivos devem ter nomes descritivos e autoexplicativos.

Funções pequenas: Devem fazer apenas uma coisa, de forma clara e eficiente.

Evite comentários desnecessários: Prefira escrever código autoexplicativo. Use comentários apenas quando realmente necessário.

Padrão de documentação: Utilize o formato Doxygen para gerar documentação automática.

Tratamento de erros: Lide com falhas de forma explícita. Não ignore exceções nem retorne códigos genéricos.

Remoção de código morto: Nunca mantenha trechos de código comentado ou não utilizados no repositório.

Eficiência: Evite chamadas desnecessárias, otimize alocação de memória e minimize overhead.



---

2. Arquitetura de Software: Clean Architecture

Regras de negócio independentes de detalhes técnicos.

Organize o projeto nas seguintes camadas:

1. Entities (Entidades): Representam as regras de negócio e o núcleo do domínio. Totalmente independentes de frameworks e bibliotecas.


2. Use Cases (Casos de uso): Orquestram o fluxo de regras de negócio. Conhecem as entidades e interagem com elas.


3. Interface Adapters: Adaptam as entradas e saídas (UI, sensores, protocolos) ao formato utilizado pelos casos de uso.


4. Frameworks e Drivers: Componentes externos como banco de dados, APIs, hardware, etc. Dependem das camadas internas, nunca o contrário.




---

3. Princípios SOLID

Aplique os princípios SOLID para manter o código modular e extensível:

S – SRP: Uma única responsabilidade por classe/módulo.

O – OCP: Código aberto para extensão, fechado para modificação.

L – LSP: Subtipos devem ser substituíveis por seus tipos base.

I – ISP: Separe interfaces grandes em menores e mais específicas.

D – DIP: Dependa de abstrações, não de implementações concretas.



---

4. Paradigmas da Programação Orientada a Objetos

Utilize os conceitos fundamentais da OOP de forma consciente:

Encapsulamento: Esconda os detalhes internos. Exponha apenas o necessário.

Herança: Reutilize código com cautela. Prefira composição sempre que possível.

Polimorfismo: Use interfaces e classes base para abstrair o comportamento.

Abstração: Foque no que é essencial. Oculte complexidade desnecessária.



---

5. Leitura de Sensores e Gerenciamento de Estado

Módulo de controle é responsável por gerenciar sensores.

As funções que realizam a leitura física dos sensores não devem ser acessíveis diretamente por outros módulos.

Cada sensor deve manter seu próprio valor atualizado em uma variável interna.

Forneça métodos de leitura indiretos, como getTemperature(), que apenas retornam o valor já armazenado, sem realizar nova leitura.

Atualizações devem ser feitas internamente em momentos definidos (ex: via polling, interrupção ou temporizador).



---

6. Concorrência e Gerenciamento de Recursos

Proteja acesso a recursos compartilhados (uso de mutex, semaphores ou std::atomic onde apropriado).

Evite vazamentos de memória: Utilize RAII, std::unique_ptr, std::shared_ptr, std::vector, etc.

Garanta desalocação segura de objetos e buffers.

Evite uso de ponteiros crus quando possível.



---

7. Controle de Versão e Commits

Utilize o padrão Conventional Commits:

Exemplo: feat(sensor): adicionar leitura de temperatura com filtro de ruído


Inclua emojis relevantes no início de cada commit para fácil identificação visual (ex: ⚙️, 🐛, ✨, 📚, etc).

Evite commits com muitos arquivos. Separe alterações por responsabilidade.

Faça commits sempre que concluir uma etapa relevante ou quando solicitado.
De o comando completo, git add, git commit...



---

8. Outros pontos importantes

Evite acoplamento excessivo entre módulos.

Prefira interfaces bem definidas para comunicação entre camadas.

Escreva código defensivo, validando entradas e condições inesperadas.

Organize o projeto em diretórios coerentes com a arquitetura em camadas.

Nomes de arquivos devem refletir suas responsabilidades.