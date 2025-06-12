===========================
CSD developer documentation
===========================

Main state machine
==================

The CSD driver uses a state machine to track the overall state of serializer and CSD.

.. kernel-figure:: states.dot
   :alt: main state machine

The WAKEUP state shown in main state diagram for simplicity actually summarizes a number of similar states traversed during display wakeup as shown in the wakeup states diagram. States are color coded as described further down. The green edges represent the path, which is taken during initial start-up. Red edges indicate some kind of error handling.

As hint for user diagnostics experience optimisation, green states, where the driver in typical use cases spends a lot of time, are filled with light green. Not filled green states are usually left quite soon, but it depends on use case. Especially in error cases the driver may stay long also in unfilled states.

.. kernel-figure:: wakeup.dot
   :alt: wakeup substates

Blue states
-----------

Blue states are transient states. The device stays only for short time periods in this states. There is no need to process other events while being in this states. This states are represented explicitly just because there are multiple ways to enter them.

SRECV_CRESET
~~~~~~~~~~~~

In this state the serializer is configured and receiving video input, while CSD is still hold in reset state. This state is left by powering on CSD.

RESETTING_CSD
~~~~~~~~~~~~~

This state is entered, when a reset of the CSD is performed without resetting the serializer. The state is left unconditionally after a short delay, which ensures a proper reset pulse on the CSD control line.

RESET_SER
~~~~~~~~~

This state is entered when a reset of serializer seems appropriate in order to recover from serious error conditions. This state is used at run-time when video input to serializer is enabled, which may require special handling depending on serializer. Without video input being enabled, any serializer can be just restarted by toggling power down line and reconfiguring. When run-time recovery was successful a transition to SRECV_CRESET is done.

SUSPENDED
~~~~~~~~~

Whenever a suspend request is received the CSD and the serializer are powered down and the serdev device is closed. This state is entered once all preparations for suspend state are completed. When a resume request is received the serdev device is reopened and this state is left.

TERMINATE
~~~~~~~~~

This state is entered if the driver is going to be unloaded or unbound from the serial interface or if some serious non-recoverable error is detected. For example in case the communication with the serializer is not working and it was not possible to recover it after many retries or if the serdev device can not be accessed after suspend. Any problems with CSD including problems with deserializer might be serious but are not considered non-recoverable as the CSD could be replaced with another working one at run-time. This state is not processing any further events and can not be left. All pending events must be processed and further events must be blocked before entering this state. The main event processing thread is terminated.

Green states
------------

Green states are states, in which the device can stay for significant amount of time (outside of small ms range). Incoming events need to be processed in this states.

PROBED_CRESET
~~~~~~~~~~~~~

This is the initial state. After basic initialization like memory allocation, initialization of synchronization primitives, GPIO allocation and irq registration the serializer is powered up, the device ID is read and configuration is performed by the probe function. After that kernel and user space interfaces are registered. Finally the main event processing thread, which implements the main state machine is started by the probe function as last action of the probe function. The main thread starts in this state. It is expected that video output on SOC is not enabled. It is expected that a pre-enable request is received very soon, therefore the serializer is kept powered on and configured. In case a pre-enable request is not received within a given timeout, serializer is powered down transitioning to SOFF_CRESET state.

SOFF_CRESET
~~~~~~~~~~~

This state simply represents the situation where the serializer and CSD are both off. When a pre-enable request is received the serilizer is powered on and configuration is performed. Transitions to other states are performed depending on success of configuration. At this point it is not known if an actual CSD or a converter board is attached. The only known difference in configuration between actual CSD and converter board is the color mapping. It is preferable to configure the color mapping matching the converter board at this point as the converter board may start output as soon as a link is established, while CSD needs to complete the wakeup sequence and needs to be switched to normal mode before displaying anything. When we successfully complete the wakeup sequence, we know for sure that there is an actual CSD attached. We can than reconfigure the color mapping before switching to normal mode.

SCONF_CRESET
~~~~~~~~~~~~

In this state the serializer is on and completely configured except for settings, which require video input to serializer to be present before the setting can be performed. After receiving an enable request the remaining configuration is done.

CSD_POWERING_ON
~~~~~~~~~~~~~~~

This state is entered, when CSD is powered on by asserting the control signal. The CSD controller is supposed to start-up and to power on the deserializer at this point. While not documented it is likely that the CSD controller also performs basic configuration of the deserializer at this point. At least it needs to configure transmission of the GPIO state of the GPIO, which is used for the interrupt signal. Additional configuration might also be required to establish the LVDS link at all. To signal completion of this process CSD controller is supposed to assert the interrupt line. According to the specification this process may take up to 8 seconds. It is not allowed to send any messages over UART during this period. Once the interrupt signal is received, an Interrupt Status Information request is send. A state transition is performed based on the received reply.

CONVERTER
~~~~~~~~~

This state is entered, when a converter board is detected.

WAIT_CONVERTER
~~~~~~~~~~~~~~

This state is entered, when we know that a converter board is supposed to be attached, but we have not yet detected it.

CSD_PBL
~~~~~~~

This state is entered from CSD_POWERING_ON state, when PBL is received as response to the Interrupt Status Information request. In this state the CSD controller is performing configuration of the deserializer, which may take up to 8 seconds according to specification. Completion of the configuration is signaled by deasserting the interrupt line.

CSD_PBL_APP_EARLY / CSD_PBL_APP_LATE
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In this states the CSD controller is switching from the Primary Boot Loader (PBL) to Application (APP) firmware. Once completed the CSD will assert the interrupt line. This should take a little bit more than 40 ms. As response an Interrupt Status Information request is send. Depending on the reply the state is switched. E.g. due to an interrupted firmware download to CSD the firmware on CSD might be broken. In order to support recovery of CSDs with broken firmware this phase is split into two states. During CSD_PBL_APP_EARLY it is assumed that the CSD firmware is working properly. If the expected interrupt is however not received within the first half of the total timeout for this phase a switch to CSD_PBL_APP_LATE is performed. In this state it is assumed that the firmware is broken. The behavior in both states is identical except that in the later state a switch to CSD_PROG is done when a programming session request is in the event queue. This way another firmware download sequence can be done.

CSD_APP
~~~~~~~

In this state the CSD controller is configuring the deserializer. It will deassert the interrupt line once done. The process is allowed to take up to 8 seconds.

IDLE
~~~~

This is the main state active most of the time during normal operation of CSD. In addition to all the other events, which are also processed in other states, this state additionally performs periodic operations according to CSD specification in order to process touch input, to adjust brightness and to check display state. Further details are described further down. In case transmission of any of the periodic messages fails, a retry is not performed as the messages are periodic anyway. Only in case many transmissions fail consecutively a reset of display and serializer is performed.

CSD_PROG
~~~~~~~~

This state is used, when the CSD is in programming mode. It is very similar to the IDLE state, but it doesn't perform any periodic operations. Touch input is not processed in this state and display status is not exposed to the diagnostic part of the user space sysfs interface. A user space application using the cdev based diagnostic interface needs to ensure that messages are send periodically in order to keep CSD in this state for a longer time period. If there is no communication CSD will perform a reset automatically.

CSD_REG_PROG
~~~~~~~~~~~~

CSD may need to access deserializer registers during normal operation. To prevent collisions on the UART lines CSD informs the driver that it wants to use the UART before performing any access. In order to inform the driver the CSD asserts the interrupt line and reports a register access as reason for the interrupt. Once this message is received the driver is not allowed to access the UART as long as the interrupt line is asserted. To reflect this situation this state is entered. As the register access is allowed to take up to several seconds the driver needs to process high priority events like unload or suspend requests during this period. The usual periodic messages obviously can not be transmitted. All events requiring UART access can not be performed during this period. Simply rejecting such events is however not reasonable, as it is very likely that the register access will be completed soon. Therefore incoming events, which require UART access, are postponed in this state.

CSD_PROG_REG_PROG
~~~~~~~~~~~~~~~~~

This state fulfills the same purpose as the CSD_REG_PROG state. The difference is that the CSD is in programming session instead of the normal or extended session.

State conditions
----------------

The table below summarizes the conditions present in each state. By definition the conditions need to be met when the state is entered i.e. a state needs to ensure all conditions are met before transitioning to the corresponding state.

.. flat-table:: State conditions

   * - state
     - serializer
     - CSD control
     - CSD state
     - int irq
     - events processed
     - diag sysfs
     - touch_down

   * - PROBED_CRESET
     - on
     - off
     - off
     - masked
     - all
     - no
     - false

   * - SOFF_CRESET
     - off
     - off
     - off / shutting down
     - masked
     - all
     - no
     - false

   * - SCONF_CRESET
     - on
     - off
     - off / shutting down
     - masked
     - all
     - no
     - false

   * - SRECV_CRESET
     - on
     - off
     - off
     - masked
     - none
     - no
     - false

   * - CSD_POWERING_ON
     - on
     - on
     - on / starting up
     - unmasked
     - all
     - no
     - false

   * - CSD_PBL
     - on
     - on
     - on
     - unmasked
     - all
     - no
     - false

   * - CSD_PBL_APP
     - on
     - on
     - on
     - unmasked
     - all
     - no
     - false

   * - CSD_APP
     - on
     - on
     - on
     - unmasked
     - all
     - no
     - false

   * - IDLE
     - on
     - on
     - on
     - unmasked
     - all
     - yes
     - true/false

   * - CSD_PROG
     - on
     - on
     - on
     - unmasked
     - all
     - no
     - false

   * - RESETTING_CSD
     - on
     - off
     - off / shutting down
     - unmasked
     - none
     - no
     - false

   * - CONVERTER
     - on
     - off
     - off
     - masked
     - all
     - no
     - false

   * - WAIT_CONVERTER
     - on
     - off
     - off
     - masked
     - all
     - no
     - false

   * - RESET_SER
     - on
     - off
     - off / shutting down
     - masked
     - none
     - no
     - false

   * - SUSPENDED
     - off
     - off
     - off / shutting down
     - masked
     - none
     - no
     - false

   * - TERMINATE
     - off
     - off
     - off / shutting down
     - blocked
     - blocked
     - no
     - false

   * - CSD_REG_PROG
     - on
     - on
     - on
     - unmasked
     - all
     - yes
     - true/false

   * - CSD_PROG_REG_PROG
     - on
     - on
     - on
     - unmasked
     - all
     - no
     - false

Notes:

"diag sysfs" refers to the diagnostic part of the sysfs interface provided to user space and indicates whether display status is exposed to the diagnost sysfs interface.

The "CSD control" column reflects the state of the CSD control line / wakeup signal currently set by SOC. Depending on the signal path between SOC and CSD it may take significant time until a state change on the control line on SOC side is actually propagated to CSD. Once CSD sees the state change it also needs some time to react on it. Depending on the exact CSD version the time required to react might be significant. According to spec CSD is supposed to enter reset state latest 10 ms after a high to low transition on the control signal. In practice it may take even much longer. Introducing such long delays in CSD driver delaying also unrelated events is not desirable. Therefore the time when the control line was set low in order to put CSD in reset is tracked explicitly and a delay making sure CSD has actually reached reset state is performed only when an event needs to be processed, which actually depends on CSD being in reset state. The "CSD state" column shows in which states a power down of CSD might be pending, i.e. where a delay might be required before certain tasks are performed.

Receiver state machine
======================

CSD controller is allowed to perform read and write accesses to deserializer. Before doing this the driver will be informed by CSD to avoid collisions on the UART. The driver is not allowed to perform any UART access as long as CSD keeps the interrupt line asserted. All messages seen on the UART during this period are intended for the deserializer and need to be ignored by the driver. Due to buffering in UART controller and kernel it is however not easily possible to determine for sure whether a particular byte was received before or after the interrupt line was deasserted. To avoid any race conditions the driver parses all messages seen on the UART and drops messages intended for the deserializer based on message content. To perform this filtering a state machine is used as shown below.

.. kernel-figure:: receiver.dot
   :alt: receiver state machine

The green process describes the path taken when a message is received which was expected by the driver and is intended for the driver. The blue paths are taken when a message is parsed which is intended for the deserializer. Red transitions are taken in case of error conditions e.g. in case of transmission errors which prevent proper parsing of the message. A re-synchronization will be attempted on the next ACK byte shown as black transitions. Re-synchronizing just based on an ACK byte is of course not reliable as an ACK byte may be contained within the payload as well. To ensure proper recovery synchronization is double-checked on every received checksum. In case the checksum doesn't match another re-synchronization attempt is performed by discarding all received bytes up to the next ACK byte which may or may not be already received. After receiving enough additional bytes the checksum is rechecked. This process is repeated until the checksum matches or a timeout occurs.

Synchronization
===============

The CSD driver needs to be able to handle a large number of different events as listed further down. All events can occur at almost any point in time, are asynchronous to periodic operations like touch input processing and also to each other. Different events can occur concurrently and some events can even occur multiple times in parallel. Most events require access to the UART interface and most are able to change the overall system state i.e. to trigger a transition in the main state machine of the driver. Additionally not all events can be processed in all states.

Events are passed to the driver by callbacks invoked by the kernel. E.g. a diagnostic request from a user space application will reach the driver as callback invoked by the kernel as result of the user space executing an ioctl command. An interrupt detected on a GPIO pin will reach the driver by invoking an interrupt handler in the driver. When the system wants to suspend it will invoke the suspend callback of the driver. All callbacks can obviously be executed asynchronously by different threads in the system. To ensure proper synchronization and prioritization between events the driver uses a priority queue. Whenever a callback is invoked informing the driver of some event the callback function en-queues the event in a priority queue and waits for completion of the en-queued event. The callback itself typically doesn't have to care for any synchronization. It just en-queues an event and waits.

Events en-queued in the queue are processed based on priority by a single thread referred to as main (event processing) thread in this documentation. The main thread is created by the probe function and runs until the driver is unloaded or unbound. Outside of probe and remove functions the main event processing thread is the only thread allowed to access the UART used for communication, to access GPIO pins and to change the overall driver state. To perform this operations no additional synchronization is required as it is guaranteed by design that there is only one thread performing this operations. The main thread is also responsible to perform periodic operations like touch input processing and brightness changes.

Events
------

Events currently supported by the driver are listed below sorted by priority. The main thread will always get the highest priority event from the queue and will process it. Events with equal priority are processed in FIFO fashion. In general it is possible to en-queue an event again in case it can not be processed right away or needs to be processed in multiple iterations. For example diagnostic events are currently processed in multiple iterations. The priority of the event will be adjusted by the :c:func:`csd_event_requeue` to make sure that the order in the queue is preserved.

UNLOAD
~~~~~~
This is the highest priority event. It is en-queued when the driver is going to be unloaded or unbound. This event must never be rejected as the kernel doesn't allow the remove callback to fail.

PRE_ENABLE, ENABLE, DISABLE, POST_DISABLE
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This events are en-queued when the corresponding drm_panel callbacks prepare(), enable(), disable() and unprepare() get called by the system.

SUSPEND
~~~~~~~

This events indicates that the system requested to suspend the device because it is going to suspend state. This event should never be rejected. It can be rejected but rejecting this event will lead to an abort of the system wide suspend sequence.

INT_IRQ
~~~~~~~

This event is generated whenever a rising or falling edge is detected on the interrupt line. This interrupts can be masked to avoid generation of this event if not desired.

DIAG_REPORT
~~~~~~~~~~~

This event is generated when user space performs an ioctl call on the diagnostic character device with command CSD_GET_DIAG_REPORT. A pointer to a :c:type:`struct csd_event_diag_rep_buffer <csd_event_diag_rep_buffer>` is passed along with this event and contains additional information about the diagnostic request. A diagnostic event might be processed in multiple iterations depending on the request and if retries are required. Once processing has started the priority of this event for subsequent iterations is above the priority of periodic messages to avoid sending of periodic messages between individual frames of a diagnostc request. This is not strictly forbidden by the spec but also not explicitly allowed, so it is avoided to be on the safe side. New diagnostic requests start with a priority below the priority of periodic messages to ensure touch input is working even under heavy diagnostic load.

"PERIODIC"
~~~~~~~~~~

Depending on the state a couple of periodic operations need to be processed by the driver. There is no explicit event representing periodic operations. In order to schedule a periodic operation the time when the next periodic operation should be executed is passed as timeout value to :c:func:`csd_get_event`. In case some other event is enqueued before the timeout expires the new event is returned otherwise NULL will be returned to indicate that the timeout has expired. To make sure that periodic operations are not delayed infinitely by incoming low priority events special handling is applied in case the specified timeout value has already passed. In that case only events which have a priority higher than periodic operations will be returned. In case there is no event or only events with low priority NULL will be returned. Requesting another event even if it is already known that it is time for the next periodic operation makes sure that higher priority events take precedence.

LF_READ
~~~~~~~

This event is en-queued when user space requests to read the line fault status register in serializer.

READ_HDCP
~~~~~~~~~

This event is en-queued when user space requests HDCP status. A :c:type:`struct csd_event_read_hdcp_buffer <csd_event_read_hdcp_buffer>` is passed along with this event to store the requested information.

READ_EDID
~~~~~~~~~

This event is en-queued when user space requests EDID information using sysfs interface. There is no real use case for this in production systems. This is for development only. A buffer large enough to store the complete EDID information is passed along with this event.

CTRL_TEST
~~~~~~~~~
This event is en-queued when user space requests a physical test of the CTRL (wakeup) signal path.

ERRB_TEST
~~~~~~~~~
DEPRECATED This event is en-queued when user space requests a physical test of the ERRB signal path.

GPIO_TEST
~~~~~~~~~
This event is en-queued when user space requests a physical test of all GPIOs between SOC and serializer.

EE_SER_READ, EE_SER_WRITE, EE_DES_READ, EE_DES_WRITE
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This events are en-queued when user space requests a read or write to a serializer or deserializer register. This is intended for development purpose only. A :c:type:`struct csd_event_ee_access_buffer <csd_event_ee_access_buffer>` is passed along with this events containing the address of the register and the value to write respectively space to store the obtained value.

Periodic events
===============

The CSD specification defines three messages (IHU Request, Display Status Information and Touch Status Information) which all need to be send periodically independent if there is touch input or not. The specification defines a loop time of 20 ms for each message however later on in a sequence diagram which shows the case where touch input is not present it illustrates a sequence of IHU Request, Display Status Request and Touch Panel Request showing a delay of 20 ms between each message resulting in an effective loop time of 60 ms. In order to reduce system load it seems reasonable to opt for this loop time of 60 ms. In case a touch event is indicated by CSD by asserting the interrupt line and replying with Touch Panel value to Interrupt Status Information Request the specification requires IHU to increase the polling rate for Touch Panel Request in order to get a loop time in the range between 4 ms and 10 ms (fixed to 10 ms for CSD 3.5 version). Other periodic messages should be sent between Touch Panel Requests with a distance of 20 ms between both messages resulting in a loop time of 40 ms for other messages. The end of touch input is indicated by deasserting the interrupt line.

The below diagram illustrates the implemented sequence of periodic events. In case there is no touch input indicated by CSD the green loop is executed otherwise the blue loop is executed. Orange and red edges illustrate the transitions between both loops when start or end of touch input are indicated by CSD. When start of touch input is indicated a Touch Panel Request is send followed by Display Status Request or IHU Request immediately to ensure it is not possible to end up in a loop without either of them by frequent touch input start and end transitions. When end of touch input is indicated a Touch Panel request is send to capture the final touch release message. In this case it is not required to send any other message right after the Touch Panel Request as the progress guaranty is provided by the red touch input start transitions.

.. kernel-figure:: periodic.dot
   :alt: periodic event patterns

Memory usage
============

In general all information are stored in this driver as locally as possible i.e. with the smallest possible scope and with the shortest possible life time. Information are exposed to individual driver parts only when required. Information used e.g. only by the main thread are not exposed to any callback functions. To realize this approach information are grouped into a few data structures described below. Hints where to place new variables are provided below.

Global variables
----------------

Except from module parameters which are of course global all remaining global information are stored in :c:type:`struct csd_global_data <csd_global_data>`. The amount of global information is very small and all global information are related to the implementation of the diagnostic character device(s). The kernel interface provided to create and manage character devices is working in a way that it seams reasonable to store this information on driver level and not on device level.

When to use: When a new variable is directly related to the kernel interface provided for character devices and needs to be persistent over the life time of the driver instance placing it here might be an option otherwise read on.

Device specific data
--------------------

Device specific data i.e. data bound to a specific serdev device for which the kernel called the probe function which are used by probe, the main thread and the remove function are stored in :c:type:`struct csd_data <csd_data>`. This structure is allocated and initialized by the probe function, passed to the main thread and finally also used by the remove function. Other parts of the driver i.e. callback functions are not allowed to use this data structure. This design decision makes it possible to avoid any synchronization when accessing this data structure except for the :c:type:`struct csd_drvdata <csd_drvdata>` part described below as it is always accessed only by one thread.

When to use: When a new variable is used by the probe function and some other part it will likely need to go here otherwise read on.

Driver data
-----------

The :c:type:`struct csd_drvdata <csd_drvdata>` is part of :c:type:`struct csd_data <csd_data>`. This part is exposed to callback functions by storing a pointer as private driver data in corresponding kernel devices. It can be used by multiple threads and therefore requires synchronization for every access unless the accessed data are read-only. This data structure contains for example the event queue used to pass events between callbacks and the main event processing thread. Synchronization is performed using the synchronization primitives contain in each substructure in this structure.

When to use: When a variable is used by any callback function either internally or to share data with other parts it should probably go here otherwise read on.

Data of main thread
-------------------

Data which are used exclusively by the main thread are stored in :c:type:`struct csd_state_buffer <csd_state_buffer>`.

When to use: Information which need to be persistent over state transitions but are used only by the main thread should go here otherwise read on.

Local variables
---------------

When to use: Great, if non of the previous conditions is fulfilled you can probably use a local variable on stack. When generating events from callback functions data structures passed along with the event can also be placed on stack. It just needs to be ensured that the variable doesn't go out of scope as long as the event was not completed. Once an event is completed the main thread is no longer allowed to access it and the creator of the event is responsible to dispose the event.
