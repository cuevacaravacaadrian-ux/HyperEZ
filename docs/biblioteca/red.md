# Internet: `Http`

`Http` hace peticiones a servidores web (HTTP y HTTPS): consultar APIs, descargar archivos o enviar datos. En Windows usa el sistema (WinHTTP); en Linux y macOS, la orden `curl`.

## Pedir datos

```text
var r = Http.get("https://example.com")
print(r.status, r.ok, r.body.length, r.headers["content-type"])

var repo = Http.getJson("https://api.github.com/repos/microsoft/vscode")
print(repo.full_name, repo.stargazers_count)
```

`get`, `post` y `request` devuelven `{status, ok, body, headers}`; `ok` es `true` si el código está entre 200 y 299. `getJson` y `postJson` devuelven directamente los datos convertidos (y lanzan un error si el servidor responde con un código de error).

## Enviar datos

```text
// un mapa o una lista se envía como JSON automáticamente
var respuesta = Http.postJson("https://httpbin.org/post", { jugador: "Ana", puntos: 120 })

// cabeceras y tiempo máximo de espera
var r = Http.get("https://api.ejemplo.com/datos", {
    headers: { Authorization: "Bearer MI_CLAVE" },
    timeout: 10,
})

// cualquier método
Http.request("DELETE", "https://api.ejemplo.com/partidas/7")
```

## Descargar archivos

```text
var bytes = Http.download("https://example.com/imagen.png", "descargas/imagen.png")
print("Descargados", bytes, "bytes")
```

## Construir direcciones

```hz
print("https://www.google.com/search?" + Http.query({ q: "lenguaje HyperEZ", hl: "es" }))
print(Http.encode("ñandú & café"))
```

## Errores de red

Sin conexión, con una dirección mal escrita o si el servidor no responde, `Http` lanza un error con un mensaje claro. Captúralo con `try` para que el programa siga:

```hz
try {
    Http.get("https://servidor-que-no-existe.invalid", { timeout: 5 })
} catch e {
    print("Sin conexión:", e.message.length > 0)
}
```

## Ejemplo: usar una API de inteligencia artificial

Muchas APIs de IA reciben y devuelven JSON por HTTPS, así que se usan directamente con `Http.postJson`. Guarda la clave en una variable de entorno, nunca en el código:

```text
var clave = System.env("MI_API_KEY")
var r = Http.postJson("https://api.ejemplo-ia.com/v1/chat", {
    model: "modelo",
    messages: [{ role: "user", content: "Escribe un haiku sobre HyperEZ" }],
}, { headers: { Authorization: "Bearer " + clave } })
print(r)
```

El ejemplo [examples/red/api_web.hz](../../examples/red/api_web.hz) consulta servicios web reales y funciona también sin conexión.
