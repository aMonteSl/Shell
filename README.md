
# Custom Shell in C

Este proyecto es una implementación de un intérprete de comandos (shell) funcional para sistemas operativos Linux. Fue desarrollado como parte del curso **Sistemas Operativos** y tiene como objetivo replicar las funcionalidades básicas de un shell estándar con características adicionales únicas.

## 🚀 Características principales

- **Ejecución de comandos básicos**: Soporte para ejecutar comandos con o sin argumentos.
- **Redirección de entrada y salida**:
  - Entrada: `cmd < file`
  - Salida: `cmd > file`
- **Ejecución en segundo plano**: `cmd &` permite ejecutar comandos de manera asíncrona.
- **Cambio de directorio (`cd`)**: Built-in para cambiar el directorio de trabajo.
- **Variables de entorno**:
  - Asignación: `x=y`
  - Expansión: `$x` se reemplaza con el valor de `x`.
  - Notificación de errores si la variable no existe.
- **Manejo de Here Documents**: Soporte para `HERE{` para capturar múltiples líneas de entrada hasta `}`.
- **Globbing**: Expansión de patrones como `*.c`.
- **Comandos avanzados**:
  - `ifok`: Ejecuta un comando solo si el anterior fue exitoso.
  - `ifnot`: Ejecuta un comando solo si el anterior falló.
- **Variable de estado `result`**: Almacena el código de salida del último comando ejecutado.

## 🛠️ Instalación

1. Clona este repositorio:
   ```bash
   git clone https://github.com/aMonteSl/Shell.git
   cd tu-repo
   ```
2. Compila el código:
   ```bash
   gcc -o shell shell.c
   ```
3. Ejecuta la shell:
   ```bash
   ./shell
   ```

## 📖 Uso

### Comandos básicos
```bash
ls -l
echo "Hola, mundo"
```

### Redirección
```bash
cat < input.txt > output.txt
```

### Ejecución en segundo plano
```bash
sleep 5 &
```

### Variables de entorno
```bash
PATH=/usr/bin
echo $PATH
```

### Here Document
```bash
wc -l HERE{
línea 1
línea 2
}
```

### Comandos `ifok` y `ifnot`
```bash
test -e /tmp
ifok echo "El archivo existe"
ifnot echo "El archivo no existe"
```

## 🔧 Opcionales implementados

1. **Here Documents**: Captura múltiples líneas como entrada estándar.
2. **Estado del último comando**: La variable `result` guarda el código de salida.
3. **Expansión de patrones (globbing)**: Soporte para `*.c` o `*test`.

## 📂 Estructura del proyecto

- `shell.c`: Código fuente de la shell.
- `README.md`: Documentación del proyecto.

## 📝 Notas adicionales

- Esta shell no utiliza funciones como `system()` o `execvp()` que gestionan automáticamente la variable `PATH`. La búsqueda de ejecutables en el sistema se realiza manualmente.
- El proyecto está diseñado para ser extensible y fácilmente integrable en sistemas UNIX.

## 🌟 Sobre el autor

Este proyecto fue desarrollado por **Adrián Montes Linares** como parte del curso de Sistemas Operativos. Para más información, puedes contactarme en [LinkedIn](https://linkedin.com/in/adrianmonteslinares).

---

¡Gracias por explorar este proyecto! Si te resulta útil o interesante, no dudes en darle una estrella ⭐ en GitHub.