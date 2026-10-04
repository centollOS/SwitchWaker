# Icono en la pantalla HOME (forwarder NSP)

`build/forwarder/centollos_forwarder.nsp` instala el icono "centollOS" en la
pantalla HOME. Al abrirlo arranca `sdmc:/switch/centollos/centollos.nro` como
aplicación, con toda la memoria: ya no hace falta abrir hbmenu manteniendo R sobre un juego.
En `centollos.log` debe aparecer `application (title mode)`.

Se genera con `scripts/switch/build_forwarder.sh` (claves por defecto en `~/.switch/prod.keys`;
nunca se copian al repositorio). Title ID: `01FF43454E540000`.

## Requisitos

- Atmosphère con sigpatches al día para tu firmware (el NSP no lleva la firma oficial de
  la consola). Si la instalación falla por la firma o el icono da un error al abrirlo, actualiza los
  sigpatches (incluidos los del loader/ACID).
- El NRO **debe** seguir en `sdmc:/switch/centollos/centollos.nro`. El forwarder solo
  contiene un cargador: si mueves o renombras el NRO, el icono dará un error al abrirlo. Para
  actualizar el juego basta con sustituir el NRO en esa ruta; no hace falta reinstalar el NSP
  (solo si cambia el icono, el nombre o la versión que se muestra).

## Instalar con DBI (USB/MTP)

1. Copia `centollos_forwarder.nsp` al ordenador.
2. En la Switch abre DBI y elige **Run MTP responder**.
3. Conecta la consola por USB. En el ordenador aparece un dispositivo MTP: abre
   **"SD Card install"** (o "NAND install") y arrastra el `.nsp` ahí.
4. Espera a que DBI termine, sal de DBI y vuelve a HOME: aparece el icono.

En macOS hace falta un cliente MTP (por ejemplo OpenMTP o Android File Transfer).

## Instalar con Goldleaf

1. Copia el `.nsp` a la tarjeta SD (por ejemplo a `sdmc:/nsp/`).
2. Abre Goldleaf → **Explorar la tarjeta SD** → selecciona el `.nsp` → **Instalar** → destino
   **Tarjeta SD**. Ignora el aviso de que no es un título oficial.
3. Vuelve a HOME. Puedes borrar el `.nsp` de la SD después.

(Tinfoil también sirve: "File browser" → el `.nsp` → Install.)

## Si tenías instalado el forwarder anterior

El forwarder anterior usaba otro title ID (`01FF57574E000000`) y abría el NRO con su nombre
antiguo. Bórralo desde HOME (Gestionar software) o desde DBI/Goldleaf por ese title ID; tus
datos de la SD no se tocan. Después instala este.

## Desinstalar

- Desde HOME: Configuración del sistema → Gestión de datos → Gestionar software → "centollOS" → **Eliminar software**. O selecciona el icono, pulsa + (Opciones) →
  Gestión de datos → Eliminar software.
- O desde DBI/Goldleaf: lista de títulos instalados → `01FF43454E540000` → borrar.

Desinstalar el icono no toca el NRO, la ISO ni los datos de `sdmc:/switch/centollos/`.

## Aviso: riesgo de baneo

Instalar NSP de homebrew deja el título en el registro de la consola, y el fabricante de la consola puede
detectarlo al conectarse a sus servidores. **No te conectes online con el forwarder instalado**
(usa emuMMC sin conexión, o bloquea los servidores oficiales con DNS/90DNS o el
`hosts` de Atmosphère). Usarlo es bajo tu propia responsabilidad.
