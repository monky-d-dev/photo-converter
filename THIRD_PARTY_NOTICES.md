# Componentes de terceros

Photo Converter utiliza Qt 6 mediante bibliotecas dinámicas. Qt y sus componentes conservan sus propias licencias; la licencia MIT de esta aplicación no las sustituye.

- Qt: https://www.qt.io/licensing/open-source-lgpl-obligations
- Licencias de Qt: https://doc.qt.io/qt-6/licenses-used-in-qt.html
- Fuentes de la versión usada en el workflow (6.8.3): https://download.qt.io/archive/qt/6.8/6.8.3/single/
- Herramienta de despliegue: https://doc.qt.io/qt-6/windows-deployment.html

El script de distribución conserva las DLL independientes y copia los archivos de licencia del kit Qt cuando están incluidos. Para redistribuir el paquete deben acompañarse las licencias de todos los componentes utilizados y cumplirse sus condiciones. El kit puede utilizarse bajo la licencia comercial de Qt cuando corresponda.
