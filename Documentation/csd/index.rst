==========
CSD driver
==========

The primary entertainment display in cars manufactured by Volvo Cars is referred to as Center Stack Display (CSD). The display is used for applications like navigation, music playback and car climate control. The display has two physically separated connections to the Integrated Head Unit (IHU). One of them is a LVDS connection using the GMSL protocol. The other connector is used mainly for power supply but contains also a so called control signal, which acts as a reset line for the display controller inside CSD and another dedicated line indicating the state of the physical home button on the display.

The actual video signal is initially emitted on a HDMI or DSI interface internally in IHU. It is converted to GMSL protocol using a serializer from Maxim Integrated (now part of Analog Devices) and send to CSD. Inside CSD the signal is received by a compatible deserializer from Maxim Integrated. There are multiple versions of the CSD with different physical sizes and display resolutions as described in the documents listed below. Depending on the used display either GMSL1 or GMSL2 protocol needs to be used.

The driver described in this document is designed to setup the LVDS communication, i.e. to configure the serializer, to turn on the display and to perform all tasks required for normal operation like periodic processing of touch input and adjustments to display brightness as requested by user space. The configuration of the deserializer is performed by the display controller itself.

While data are transmitted over HDMI / DSI to serializer, control signals are transmitted over UART. The same UART is used to interact with serializer, deserializer and CSD controller. Usually it would be preferable to have multiple smaller drivers for the individual components, like serializer, deserializer, CSD controller and touch controller. However due to the used protocol it seems reasonable to handle all parts in one driver. Having separate drivers for serializer and CSD controller would make it difficult to handle timing dependencies between both. The CSD controller is using the same UART interface to communicate with the deserializer as used to communicate with serializer and CSD controller. To prevent collisions on the UART, which is tunneled over LVDS, it needs to be ensured that only one component is accessing the UART at any given time. Given the protocol this is much easier to accomplish using one driver. Having a separate driver for the deserializer wouldn't be really useful, as it is controlled mainly from CSD controller anyway. Having a separate driver for the touch controller is not reasonable, as the CSD controller doesn't expose the touch controller to IHU. It is just forwarding touch reports in a custom format.

The driver is designed to support any IHU 4.0 and IHU 4.2 hardware starting from IHU 4.0 revision 10 onward and is intended to comply with the following documents from Volvo Cars:

- LVDS COMMUNICATION PROTOCOL, document number 31842101, volume 01, revision 022, release date 2018-12-21 (level 1 displays are not supported)

- LVDS COMMUNICATION PROTOCOL CSD 3.5, document number 33688389, volume 01, revision 004, release date 2019-06-25

User guides for CSD
===================

.. toctree::
   :maxdepth: 2

   user

Kernel developers
=================

.. toctree::
   :maxdepth: 2

   developer
   code
