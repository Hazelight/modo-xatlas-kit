# Modo xatlas Kit

This kit includes a plug-in command `xatlas` - that will create uvs or pack existing ones using [xatlas](https://github.com/jpcy/xatlas)

Most arguments should be self-explanatory. Only one I found misleading was 'Rotate Charts to Axis' which will not actually rotate - but flip the chart if it would result in a more optimal packing solution.

## Development

To build this project using cmake, set the LXSDK_PATH to where you downloaded the lxsdk. And XA_PATH to the root directory of xatlas.

## Glossary

Chart - Group of connected polygons
LSCM - Least Squares Conformal Maps
