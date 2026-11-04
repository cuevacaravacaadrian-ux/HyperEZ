# Publicar la extensión en el Marketplace de VS Code

El archivo listo para publicar es `dist/hyperez-<versión>.vsix` (lo crea `./build.ps1`).

## Opción A: subir el .vsix desde la web (la más sencilla)

1. Entra en <https://marketplace.visualstudio.com/manage> con una cuenta de Microsoft.
2. La primera vez, pulsa **Create publisher**:
   - **ID**: `hyperez` (es el que figura en `package.json` como `"publisher"`; si ya está cogido, elige otro y cámbialo también en `package.json` antes de compilar).
   - **Name**: el nombre que se mostrará, por ejemplo `HyperEZ`.
3. Dentro del editor, pulsa **New extension → Visual Studio Code** y sube `hyperez-<versión>.vsix`.
4. En unos minutos aparece en el Marketplace y en la pestaña Extensiones de VS Code (busca "HyperEZ").

Para publicar una versión nueva: sube el `.vsix` nuevo con **Update** en el mismo sitio (la versión de `package.json` debe ser mayor; usa `python tools/set_version.py`).

## Opción B: desde la terminal con `vsce`

1. Crea un *token* de acceso en Azure DevOps (<https://dev.azure.com> → *User settings → Personal access tokens*):
   - **Organization**: *All accessible organizations*.
   - **Scopes**: *Custom defined → Marketplace → Manage*.
2. Desde `editors/vscode`:

```text
npx vsce login hyperez          pega el token cuando lo pida
npx vsce publish                compila y publica la versión de package.json
```

## Open VSX (VSCodium, Gitpod, Cursor...)

```text
npx ovsx create-namespace hyperez -p <token de open-vsx.org>
npx ovsx publish hyperez-0.7.0.vsix -p <token>
```

## Antes de publicar

- Cambia `"publisher"` en `package.json` si tu ID no es `hyperez`. El identificador de la extensión pasa a ser `<publisher>.hyperez`: si tenías instalada la anterior, desinstálala para que no haya dos.
- Las imágenes y enlaces del README deben ser accesibles públicamente: si el repositorio de GitHub es privado, los enlaces a él no se podrán abrir desde el Marketplace.
- Comprueba el paquete: `npx vsce ls` lista los archivos que incluye.
