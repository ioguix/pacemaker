Advanced Configuration
----------------------

.. index::
   single: start-delay; operation attribute
   single: interval-origin; operation attribute
   single: interval; interval-origin
   single: operation; interval-origin
   single: operation; start-delay

Specifying When Recurring Actions are Performed
###############################################

By default, recurring actions are scheduled relative to when the resource
started. In some cases, you might prefer that a recurring action start relative
to a specific date and time. For example, you might schedule an in-depth
monitor to run once every 24 hours, and want it to run outside business hours.

To do this, set the operation's ``interval-origin``. The cluster uses this point
to calculate the correct ``start-delay`` such that the operation will occur
at ``interval-origin`` plus a multiple of the operation interval.

For example, if the recurring operation's interval is 24h, its
``interval-origin`` is set to 02:00, and it is currently 14:32, then the
cluster would initiate the operation after 11 hours and 28 minutes.

The value specified for ``interval`` and ``interval-origin`` can be any
date/time conforming to the
`ISO8601 standard <https://en.wikipedia.org/wiki/ISO_8601>`_. By way of
example, to specify an operation that would run on the first Monday of
2021 and every Monday after that, you would add:

.. topic:: Example recurring action that runs relative to base date/time

   .. code-block:: xml

      <op id="intensive-monitor" name="monitor" interval="P7D" interval-origin="2021-W01-1"/>

.. index::
   single: resource; failure recovery
   single: operation; failure recovery

.. _failure-handling:

Handling Resource Failure
#########################

By default, Pacemaker will attempt to recover failed resources by restarting
them. However, failure recovery is highly configurable.

.. index::
   single: resource; failure count
   single: operation; failure count

Failure Counts
______________

Pacemaker tracks resource failures for each combination of node, resource, and
operation (start, stop, monitor, etc.).

You can query the fail count for a particular node, resource, and/or operation
using the ``crm_failcount`` command. For example, to see how many times the
10-second monitor for ``myrsc`` has failed on ``node1``, run:

.. code-block:: none

   # crm_failcount --query -r myrsc -N node1 -n monitor -I 10s

If you omit the node, ``crm_failcount`` will use the local node. If you omit
the operation and interval, ``crm_failcount`` will display the sum of the fail
counts for all operations on the resource.

You can use ``crm_resource --cleanup`` or ``crm_failcount --delete`` to clear
fail counts. For example, to clear the above monitor failures, run:

.. code-block:: none

   # crm_resource --cleanup -r myrsc -N node1 -n monitor -I 10s

If you omit the resource, ``crm_resource --cleanup`` will clear failures for
all resources. If you omit the node, it will clear failures on all nodes. If
you omit the operation and interval, it will clear the failures for all
operations on the resource.

.. note::

   Even when cleaning up only a single operation, all failed operations will
   disappear from the status display. This allows us to trigger a re-check of
   the resource's current status.

Higher-level tools may provide other commands for querying and clearing
fail counts.

The ``crm_mon`` tool shows the current cluster status, including any failed
operations. To see the current fail counts for any failed resources, call
``crm_mon`` with the ``--failcounts`` option. This shows the fail counts per
resource (that is, the sum of any operation fail counts for the resource).

.. index::
   single: migration-threshold; resource meta-attribute
   single: resource; migration-threshold

Failure Response
________________

Normally, if a running resource fails, pacemaker will try to stop it and start
it again. Pacemaker will choose the best location to start it each time, which
may be the same node that it failed on.

However, if a resource fails repeatedly, it is possible that there is an
underlying problem on that node, and you might desire trying a different node
in such a case. Pacemaker allows you to set your preference via the
``migration-threshold`` resource meta-attribute. [#]_

If you define ``migration-threshold`` to *N* for a resource, it will be banned
from the original node after *N* failures there.

.. note::

   The ``migration-threshold`` is per *resource*, even though fail counts are
   tracked per *operation*. The operation fail counts are added together
   to compare against the ``migration-threshold``.

By default, fail counts remain until manually cleared by an administrator
using ``crm_resource --cleanup`` or ``crm_failcount --delete`` (hopefully after
first fixing the failure's cause). It is possible to have fail counts expire
automatically by setting the ``failure-timeout`` resource meta-attribute.

.. important::

   A successful operation does not clear past failures. If a recurring monitor
   operation fails once, succeeds many times, then fails again days later, its
   fail count is 2. Fail counts are cleared only by manual intervention or
   falure timeout.

For example, setting ``migration-threshold`` to 2 and ``failure-timeout`` to
``60s`` would cause the resource to move to a new node after 2 failures, and
allow it to move back (depending on stickiness and constraint scores) after one
minute.

.. note::

   ``failure-timeout`` is measured since the most recent failure. That is, older
   failures do not individually time out and lower the fail count. Instead, all
   failures are timed out simultaneously (and the fail count is reset to 0) if
   there is no new failure for the timeout period.

There are two exceptions to the migration threshold: when a resource either
fails to start or fails to stop.

If the cluster property ``start-failure-is-fatal`` is set to ``true`` (which is
the default), start failures cause the fail count to be set to ``INFINITY`` and
thus always cause the resource to move immediately.

Stop failures are slightly different and crucial.  If a resource fails to stop
and fencing is enabled, then the cluster will fence the node in order to be
able to start the resource elsewhere.  If fencing is disabled, then the cluster
has no way to continue and will not try to start the resource elsewhere, but
will try to stop it again after any failure timeout or clearing.

.. index::
   single: resource; move

Moving Resources
################

Moving Resources Manually
_________________________

There are primarily two occasions when you would want to move a resource from
its current location: when the whole node is under maintenance, and when a
single resource needs to be moved.

.. index::
   single: standby mode
   single: node; standby mode

Standby Mode
~~~~~~~~~~~~

Since everything eventually comes down to a score, you could create constraints
for every resource to prevent them from running on one node. While Pacemaker
configuration can seem convoluted at times, not even we would require this of
administrators.

Instead, you can set a special node attribute which tells the cluster "don't
let anything run here". There is even a helpful tool to help query and set it,
called ``crm_standby``. To check the standby status of the current machine,
run:

.. code-block:: none

   # crm_standby -G

A value of ``on`` indicates that the node is *not* able to host any resources,
while a value of ``off`` says that it *can*.

You can also check the status of other nodes in the cluster by specifying the
`--node` option:

.. code-block:: none

   # crm_standby -G --node sles-2

To change the current node's standby status, use ``-v`` instead of ``-G``:

.. code-block:: none

   # crm_standby -v on

Again, you can change another host's value by supplying a hostname with
``--node``.

A cluster node in standby mode will not run resources, but still contributes to
quorum, and may fence or be fenced by nodes.

Moving One Resource
~~~~~~~~~~~~~~~~~~~

When only one resource is required to move, we could do this by creating
location constraints.  However, once again we provide a user-friendly shortcut
as part of the ``crm_resource`` command, which creates and modifies the extra
constraints for you.  If ``Email`` were running on ``sles-1`` and you wanted it
moved to a specific location, the command would look something like:

.. code-block:: none

   # crm_resource -M -r Email -H sles-2

Behind the scenes, the tool will create the following location constraint:

.. code-block:: xml

   <rsc_location id="cli-prefer-Email" rsc="Email" node="sles-2" score="INFINITY"/>

It is important to note that subsequent invocations of ``crm_resource -M`` are
not cumulative. So, if you ran these commands:

.. code-block:: none

   # crm_resource -M -r Email -H sles-2
   # crm_resource -M -r Email -H sles-3

then it is as if you had never performed the first command.

To allow the resource to move back again, use:

.. code-block:: none

   # crm_resource -U -r Email

Note the use of the word *allow*.  The resource *can* move back to its original
location, but depending on ``resource-stickiness``, location constraints, and
so forth, it might stay where it is.

To be absolutely certain that it moves back to ``sles-1``, move it there before
issuing the call to ``crm_resource -U``:

.. code-block:: none

   # crm_resource -M -r Email -H sles-1
   # crm_resource -U -r Email

Alternatively, if you only care that the resource should be moved from its
current location, try:

.. code-block:: none

   # crm_resource -B -r Email

which will instead create a negative constraint, like:

.. code-block:: xml

   <rsc_location id="cli-ban-Email-on-sles-1" rsc="Email" node="sles-1" score="-INFINITY"/>

This will achieve the desired effect, but will also have long-term
consequences. As the tool will warn you, the creation of a ``-INFINITY``
constraint will prevent the resource from running on that node until
``crm_resource -U`` is used. This includes the situation where every other
cluster node is no longer available!

In some cases, such as when ``resource-stickiness`` is set to ``INFINITY``, it
is possible that you will end up with the problem described in
:ref:`node-score-equal`. The tool can detect some of these cases and deals with
them by creating both positive and negative constraints. For example:

.. code-block:: xml

   <rsc_location id="cli-ban-Email-on-sles-1" rsc="Email" node="sles-1" score="-INFINITY"/>
   <rsc_location id="cli-prefer-Email" rsc="Email" node="sles-2" score="INFINITY"/>

which has the same long-term consequences as discussed earlier.

Moving Resources Due to Connectivity Changes
____________________________________________

You can configure the cluster to move resources when external connectivity is
lost in two steps.

.. index::
   single: ocf:pacemaker:ping resource
   single: ping resource

Tell Pacemaker to Monitor Connectivity
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

First, add an ``ocf:pacemaker:ping`` resource to the cluster. The ``ping``
resource uses the system utility of the same name to a test whether a list of
machines (specified by DNS hostname or IP address) are reachable, and uses the
results to maintain a node attribute.

The node attribute is called ``pingd`` by default, but is customizable in order
to allow multiple ping groups to be defined.

Normally, the ping resource should run on all cluster nodes, which means that
you'll need to create a clone. A template for this can be found below, along
with a description of the most interesting parameters.

.. table:: **Commonly Used ocf:pacemaker:ping Resource Parameters**

   +--------------------+--------------------------------------------------------------+
   | Resource Parameter | Description                                                  |
   +====================+==============================================================+
   | dampen             | .. index::                                                   |
   |                    |    single: ocf:pacemaker:ping resource; dampen parameter     |
   |                    |    single: dampen; ocf:pacemaker:ping resource parameter     |
   |                    |                                                              |
   |                    | The time to wait (dampening) for further changes to occur.   |
   |                    | Use this to prevent a resource from bouncing around the      |
   |                    | cluster when cluster nodes notice the loss of connectivity   |
   |                    | at slightly different times.                                 |
   +--------------------+--------------------------------------------------------------+
   | multiplier         | .. index::                                                   |
   |                    |    single: ocf:pacemaker:ping resource; multiplier parameter |
   |                    |    single: multiplier; ocf:pacemaker:ping resource parameter |
   |                    |                                                              |
   |                    | The number of connected ping nodes gets multiplied by this   |
   |                    | value to get a score. Useful when there are multiple ping    |
   |                    | nodes configured.                                            |
   +--------------------+--------------------------------------------------------------+
   | host_list          | .. index::                                                   |
   |                    |    single: ocf:pacemaker:ping resource; host_list parameter  |
   |                    |    single: host_list; ocf:pacemaker:ping resource parameter  |
   |                    |                                                              |
   |                    | The machines to contact in order to determine the current    |
   |                    | connectivity status. Allowed values include resolvable DNS   |
   |                    | connectivity host names, IPv4 addresses, and IPv6 addresses. |
   +--------------------+--------------------------------------------------------------+

.. topic:: Example ping resource that checks node connectivity once every minute

   .. code-block:: xml

      <clone id="Connected">
         <primitive id="ping" class="ocf" provider="pacemaker" type="ping">
          <instance_attributes id="ping-attrs">
            <nvpair id="ping-dampen"     name="dampen" value="5s"/>
            <nvpair id="ping-multiplier" name="multiplier" value="1000"/>
            <nvpair id="ping-hosts"      name="host_list" value="my.gateway.com www.bigcorp.com"/>
          </instance_attributes>
          <operations>
            <op id="ping-monitor-60s" interval="60s" name="monitor"/>
          </operations>
         </primitive>
      </clone>

.. important::

   You're only half done. The next section deals with telling Pacemaker how to
   deal with the connectivity status that ``ocf:pacemaker:ping`` is recording.

Tell Pacemaker How to Interpret the Connectivity Data
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. important::

   Before attempting the following, make sure you understand
   :ref:`rules`.

There are a number of ways to use the connectivity data.

The most common setup is for people to have a single ping target (for example,
the service network's default gateway), to prevent the cluster from running a
resource on any unconnected node.

.. topic:: Don't run a resource on unconnected nodes

   .. code-block:: xml

      <rsc_location id="WebServer-no-connectivity" rsc="Webserver">
         <rule id="ping-exclude-rule" score="-INFINITY" >
            <expression id="ping-exclude" attribute="pingd" operation="not_defined"/>
         </rule>
      </rsc_location>

A more complex setup is to have a number of ping targets configured. You can
require the cluster to only run resources on nodes that can connect to all (or
a minimum subset) of them.

.. topic:: Run only on nodes connected to three or more ping targets

   .. code-block:: xml

      <primitive id="ping" provider="pacemaker" class="ocf" type="ping">
      ... <!-- omitting some configuration to highlight important parts -->
         <nvpair id="ping-multiplier" name="multiplier" value="1000"/>
      ...
      </primitive>
      ...
      <rsc_location id="WebServer-connectivity" rsc="Webserver">
         <rule id="ping-prefer-rule" score="-INFINITY" >
            <expression id="ping-prefer" attribute="pingd" operation="lt" value="3000"/>
         </rule>
      </rsc_location>

Alternatively, you can tell the cluster only to *prefer* nodes with the best
connectivity, by using ``score-attribute`` in the rule. Just be sure to set
``multiplier`` to a value higher than that of ``resource-stickiness`` (and
don't set either of them to ``INFINITY``).

.. topic:: Prefer node with most connected ping nodes

   .. code-block:: xml

      <rsc_location id="WebServer-connectivity" rsc="Webserver">
         <rule id="ping-prefer-rule" score-attribute="pingd" >
            <expression id="ping-prefer" attribute="pingd" operation="defined"/>
         </rule>
      </rsc_location>

It is perhaps easier to think of this in terms of the simple constraints that
the cluster translates it into. For example, if ``sles-1`` is connected to all
five ping nodes but ``sles-2`` is only connected to two, then it would be as if
you instead had the following constraints in your configuration:

.. topic:: How the cluster translates the above location constraint

   .. code-block:: xml

      <rsc_location id="ping-1" rsc="Webserver" node="sles-1" score="5000"/>
      <rsc_location id="ping-2" rsc="Webserver" node="sles-2" score="2000"/>

The advantage is that you don't have to manually update any constraints
whenever your network connectivity changes.

You can also combine the concepts above into something even more complex. The
example below shows how you can prefer the node with the most connected ping
nodes provided they have connectivity to at least three (again assuming that
``multiplier`` is set to 1000).

.. topic:: More complex example of choosing location based on connectivity

   .. code-block:: xml

      <rsc_location id="WebServer-connectivity" rsc="Webserver">
         <rule id="ping-exclude-rule" score="-INFINITY" >
            <expression id="ping-exclude" attribute="pingd" operation="lt" value="3000"/>
         </rule>
         <rule id="ping-prefer-rule" score-attribute="pingd" >
            <expression id="ping-prefer" attribute="pingd" operation="defined"/>
         </rule>
      </rsc_location>


.. _live-migration:

Migrating Resources
___________________

Normally, when the cluster needs to move a resource, it fully restarts the
resource (that is, it stops the resource on the current node and starts it on
the new node).

However, some types of resources, such as many virtual machines, are able to
move to another location without loss of state (often referred to as live
migration or hot migration). In pacemaker, this is called resource migration.
Pacemaker can be configured to migrate a resource when moving it, rather than
restarting it.

Not all resources are able to migrate; see the
:ref:`migration checklist <migration_checklist>` below. Even those that can,
won't do so in all situations. Conceptually, there are two requirements from
which the other prerequisites follow:

* The resource must be active and healthy at the old location; and
* everything required for the resource to run must be available on both the old
  and new locations.

The cluster is able to accommodate both *push* and *pull* migration models by
requiring the resource agent to support two special actions: ``migrate_to``
(performed on the current location) and ``migrate_from`` (performed on the
destination).

In push migration, the process on the current location transfers the resource
to the new location where is it later activated. In this scenario, most of the
work would be done in the ``migrate_to`` action and, if anything, the
activation would occur during ``migrate_from``.

Conversely for pull, the ``migrate_to`` action is practically empty and
``migrate_from`` does most of the work, extracting the relevant resource state
from the old location and activating it.

There is no wrong or right way for a resource agent to implement migration, as
long as it works.

.. _migration_checklist:

.. topic:: Migration Checklist

   * The resource may not be a clone.
   * The resource agent standard must be OCF.
   * The resource must not be in a failed or degraded state.
   * The resource agent must support ``migrate_to`` and ``migrate_from``
     actions, and advertise them in its meta-data.
   * The resource must have the ``allow-migrate`` meta-attribute set to
     ``true`` (which is not the default).

If an otherwise migratable resource depends on another resource via an ordering
constraint, there are special situations in which it will be restarted rather
than migrated.

For example, if the resource depends on a clone, and at the time the resource
needs to be moved, the clone has instances that are stopping and instances that
are starting, then the resource will be restarted. The scheduler is not yet
able to model this situation correctly and so takes the safer (if less optimal)
path.

Also, if a migratable resource depends on a non-migratable resource, and both
need to be moved, the migratable resource will be restarted.


.. index::
   single: node; health

.. _node-health:

Tracking Node Health
####################

A node may be functioning adequately as far as cluster membership is concerned,
and yet be "unhealthy" in some respect that makes it an undesirable location
for resources. For example, a disk drive may be reporting SMART errors, or the
CPU may be highly loaded.

Pacemaker offers a way to automatically move resources off unhealthy nodes.

.. index::
   single: node attribute; health

Node Health Attributes
______________________

Pacemaker will treat any node attribute whose name starts with ``#health`` as
an indicator of node health. Node health attributes may have one of the
following values:

.. table:: **Allowed Values for Node Health Attributes**

   +------------+--------------------------------------------------------------+
   | Value      | Intended significance                                        |
   +============+==============================================================+
   | ``red``    | .. index::                                                   |
   |            |    single: red; node health attribute value                  |
   |            |    single: node attribute; health (red)                      |
   |            |                                                              |
   |            | This indicator is unhealthy                                  |
   +------------+--------------------------------------------------------------+
   | ``yellow`` | .. index::                                                   |
   |            |    single: yellow; node health attribute value               |
   |            |    single: node attribute; health (yellow)                   |
   |            |                                                              |
   |            | This indicator is becoming unhealthy                         |
   +------------+--------------------------------------------------------------+
   | ``green``  | .. index::                                                   |
   |            |    single: green; node health attribute value                |
   |            |    single: node attribute; health (green)                    |
   |            |                                                              |
   |            | This indicator is healthy                                    |
   +------------+--------------------------------------------------------------+
   | *integer*  | .. index::                                                   |
   |            |    single: score; node health attribute value                |
   |            |    single: node attribute; health (score)                    |
   |            |                                                              |
   |            | A numeric score to apply to all resources on this node (0 or |
   |            | positive is healthy, negative is unhealthy)                  |
   +------------+--------------------------------------------------------------+


.. index::
   pair: cluster option; node-health-strategy

Node Health Strategy
____________________

Pacemaker assigns a node health score to each node, as the sum of the values of
all its node health attributes. This score will be used as a location
constraint applied to this node for all resources.

The ``node-health-strategy`` cluster option controls how Pacemaker responds to
changes in node health attributes, and how it translates ``red``, ``yellow``,
and ``green`` to scores.

Allowed values are:

.. table:: **Node Health Strategies**

   +----------------+----------------------------------------------------------+
   | Value          | Effect                                                   |
   +================+==========================================================+
   | none           | .. index::                                               |
   |                |    single: node-health-strategy; none                    |
   |                |    single: none; node-health-strategy value              |
   |                |                                                          |
   |                | Do not track node health attributes at all.              |
   +----------------+----------------------------------------------------------+
   | migrate-on-red | .. index::                                               |
   |                |    single: node-health-strategy; migrate-on-red          |
   |                |    single: migrate-on-red; node-health-strategy value    |
   |                |                                                          |
   |                | Assign the value of ``-INFINITY`` to ``red``, and 0 to   |
   |                | ``yellow`` and ``green``. This will cause all resources  |
   |                | to move off the node if any attribute is ``red``.        |
   +----------------+----------------------------------------------------------+
   | only-green     | .. index::                                               |
   |                |    single: node-health-strategy; only-green              |
   |                |    single: only-green; node-health-strategy value        |
   |                |                                                          |
   |                | Assign the value of ``-INFINITY`` to ``red`` and         |
   |                | ``yellow``, and 0 to ``green``. This will cause all      |
   |                | resources to move off the node if any attribute is       |
   |                | ``red`` or ``yellow``.                                   |
   +----------------+----------------------------------------------------------+
   | progressive    | .. index::                                               |
   |                |    single: node-health-strategy; progressive             |
   |                |    single: progressive; node-health-strategy value       |
   |                |                                                          |
   |                | Assign the value of the ``node-health-red`` cluster      |
   |                | option to ``red``, the value of ``node-health-yellow``   |
   |                | to ``yellow``, and the value of ``node-health-green`` to |
   |                | ``green``. Each node is additionally assigned a score of |
   |                | ``node-health-base`` (this allows resources to start     |
   |                | even if some attributes are ``yellow``). This strategy   |
   |                | gives the administrator finer control over how important |
   |                | each value is.                                           |
   +----------------+----------------------------------------------------------+
   | custom         | .. index::                                               |
   |                |    single: node-health-strategy; custom                  |
   |                |    single: custom; node-health-strategy value            |
   |                |                                                          |
   |                | Track node health attributes using the same values as    |
   |                | ``progressive`` for ``red``, ``yellow``, and ``green``,  |
   |                | but do not take them into account. The administrator is  |
   |                | expected to implement a policy by defining :ref:`rules`  |
   |                | referencing node health attributes.                      |
   +----------------+----------------------------------------------------------+


Configuring Node Health Agents
______________________________

Since Pacemaker calculates node health based on node attributes, any method
that sets node attributes may be used to measure node health. The most common
are resource agents and custom daemons.

Pacemaker provides examples that can be used directly or as a basis for custom
code. The ``ocf:pacemaker:HealthCPU``, ``ocf:pacemaker:HealthIOWait``, and
``ocf:pacemaker:HealthSMART`` resource agents set node health attributes based
on CPU and disk status.

To take advantage of this feature, add the resource to your cluster (generally
as a cloned resource with a recurring monitor action, to continually check the
health of all nodes). For example:

.. topic:: Example HealthIOWait resource configuration

   .. code-block:: xml

      <clone id="resHealthIOWait-clone">
        <primitive class="ocf" id="HealthIOWait" provider="pacemaker" type="HealthIOWait">
          <instance_attributes id="resHealthIOWait-instance_attributes">
            <nvpair id="resHealthIOWait-instance_attributes-red_limit" name="red_limit" value="30"/>
            <nvpair id="resHealthIOWait-instance_attributes-yellow_limit" name="yellow_limit" value="10"/>
          </instance_attributes>
          <operations>
            <op id="resHealthIOWait-monitor-interval-5" interval="5" name="monitor" timeout="5"/>
            <op id="resHealthIOWait-start-interval-0s" interval="0s" name="start" timeout="10s"/>
            <op id="resHealthIOWait-stop-interval-0s" interval="0s" name="stop" timeout="10s"/>
          </operations>
        </primitive>
      </clone>

The resource agents use ``attrd_updater`` to set proper status for each node
running this resource, as a node attribute whose name starts with ``#health``
(for ``HealthIOWait``, the node attribute is named ``#health-iowait``).

When a node is no longer faulty, you can force the cluster to make it available
to take resources without waiting for the next monitor, by setting the node
health attribute to green. For example:

.. topic:: **Force node1 to be marked as healthy**

   .. code-block:: none

      # attrd_updater --name "#health-iowait" --update "green" --node "node1"


.. index::
   single: reload

Reloading Services After a Definition Change
############################################

The cluster automatically detects changes to the configuration of active
resources. The cluster's normal response is to stop the service (using the old
definition) and start it again (with the new definition). This works, but some
resource agents are smarter and can be told to use a new set of options without
restarting.

To take advantage of this capability, the resource agent must:

* Accept the ``reload`` action and handle it appropriately. The actions
  here depend completely on your application!

  .. warning::

     Many resource agents define the ``reload`` action to make the managed
     service reload its own *native* configuration. This is not compatible with
     Pacemaker's reload feature, which requires the reload action to
     accommodate changes in the resource's *Pacemaker* configuration (that is,
     the values of the agent's reloadable parameters).

     Pacemaker is likely to use a different action name in a future release, to
     avoid conflicts with agents that perform a native reload.

* Advertise the ``reload`` operation in the ``actions`` section of its
  meta-data.

* Set the ``unique`` attribute set to 0 in the ``parameters`` section of
  its meta-data for any parameters eligible to be reloaded after a change.

  .. warning::

     Pacemaker's use of ``unique`` to indicate reloadability is non-standard and
     likely to change in a future release.

Once these requirements are satisfied, the cluster will automatically know to
reload the resource (instead of restarting) when a non-unique parameter
changes.

.. note::

   Metadata will not be re-read unless the resource needs to be started. If you
   edit the agent of an already active resource to set a parameter reloadable,
   the resource may restart the first time the parameter value changes.

.. note::

   If both a unique and non-unique parameter are changed simultaneously, the
   resource will be restarted.

.. rubric:: Footnotes

.. [#] The naming of this option was perhaps unfortunate as it is easily
       confused with live migration, the process of moving a resource from one
       node to another without stopping it.  Xen virtual guests are the most
       common example of resources that can be migrated in this manner.
