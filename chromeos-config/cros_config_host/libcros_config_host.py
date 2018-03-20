# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chrome OS Configuration access library.

Provides build-time access to the master configuration on the host. It is used
for reading from the master configuration. Consider using cros_config_host.py
for CLI access to this library.
"""

from __future__ import print_function

from collections import namedtuple, OrderedDict
import os
import sys

from . import validate_config

# Represents a single touch firmware file which needs to be installed:
#   source: source filename of firmware file. This is installed in a
#       directory in the root filesystem
#   dest: destination filename of firmware file in the root filesystem. This is
#       in /opt/google/touch/firmware
#   symlink: name of symbolic link to put in LIB_FIRMWARE to point to the touch
#       firmware. This is where Linux finds the firmware at runtime.
TouchFile = namedtuple('TouchFile', ['source', 'dest', 'symlink'])

# Represents a single file which needs to be installed:
#   source: Source filename within ${FILESDIR}
#   dest: Destination filename in the root filesystem
BaseFile = namedtuple('BaseFile', ['source', 'dest'])

# Represents information needed to create firmware for a model:
#   model: Name of model (e.g 'reef'). Also used as the signature ID for signing
#   shared_model: Name of model containing the shared firmware used by this
#       model, or None if this model has its own firmware images
#   key_id: Key ID used to sign firmware for this model (e.g. 'REEF')
#   have_image: True if we need to generate a setvars.sh file for this model.
#       If this is False it indicates that the model will never be detected at
#       run-time since it is a zero-touch whitelabel model. The signature ID
#       will be obtained from the customization_id in VPD when needed. Signing
#       instructions should still be generated for this model.
#   bios_build_target: Build target to use to build the BIOS, or None if none
#   ec_build_target: Build target to use to build the EC, or None if none
#   main_image_uri: URI to use to obtain main firmware image (e.g.
#       'bcs://Caroline.2017.21.1.tbz2')
#   ec_image_uri: URI to use to obtain the EC (Embedded Controller) firmware
#       image
#   pd_image_uri: URI to use to obtain the PD (Power Delivery controller)
#       firmware image
#   extra: List of extra files to include in the firmware update, each a string
#   create_bios_rw_image: True to create a RW BIOS image
#   tools: List of tools to include in the firmware update
#   sig_id: Signature ID to put in the setvars.sh file. This is normally the
#       same as the model, since that is what we use for signature ID. But for
#       zero-touch whitelabel this is 'sig-id-in-customization-id' since we do
#       not know the signature ID until we look up in VPD.
FirmwareInfo = namedtuple(
    'FirmwareInfo',
    ['model', 'shared_model', 'key_id', 'have_image',
     'bios_build_target', 'ec_build_target', 'main_image_uri',
     'main_rw_image_uri', 'ec_image_uri', 'pd_image_uri',
     'extra', 'create_bios_rw_image', 'tools', 'sig_id'])

UNIBOARD_DTB_INSTALL_DIR = 'usr/share/chromeos-config'

# We support two configuration file format
(FORMAT_FDT, FORMAT_YAML) = range(2)

# We allow comparing two different config formats:
# - Never compare (just use the config format provided)
# - Compare if we can find a config file for the other format
# - Require that both formats are present
(COMPARE_NEVER, COMPARE_IF_PRESENT, COMPARE_ALWAYS) = range(3)

class PathComponent(object):
  """A component in a directory/file tree

  Properties:
    name: Name this component
    children: Dict of children:
      key: Name of child
      value: PathComponent object for child
  """
  def __init__(self, name):
    self.name = name
    self.children = dict()

  def AddPath(self, path):
    parts = path.split('/', 1)
    part = parts[0]
    rest = parts[1] if len(parts) > 1 else ''
    child = self.children.get(part)
    if not child:
      child = PathComponent(part)
      self.children[part] = child
    if rest:
      child.AddPath(rest)

  def ShowTree(self, base_path, path='', indent=0):
    """Show a tree of file paths

    This shows a component and all its children. Nodes can either be directories
    or files. Each file is shown with its size, or 'missing' if not found.

    Args:
      base_path: Base path where the actual files can be found
      path: Path of this component relative to the root (e.g. 'etc/cras/)
      indent: Indent level we are up to (0 = first)
    """
    path = os.path.join(path, self.name)
    fname = os.path.join(base_path, path)
    if os.path.isdir(fname):
      status = ''
    elif os.path.exists(fname):
      status = os.stat(fname).st_size
    else:
      status = 'missing'
    print('%-10s%s%s%s' % (status, '   ' * indent, str(self.name),
                           self.children and '/' or ''))
    for child in sorted(self.children.keys()):
      self.children[child].ShowTree(base_path, path, indent + 1)


def GetFilename(node_path, props, fname_template):
  """Create a filename based on the given template and properties

  Args:
    node_path: Path of the node generating this filename (for error
        reporting only)
    props: Dict of properties which can be used in the template:
        key: Variable name
        value: Value of that variable
    fname_template: Filename template
  """
  template = fname_template.replace('$', '')
  try:
    return template.format(props, **props)
  except KeyError as e:
    raise ValueError(("node '%s': Format string '%s' has properties '%s' " +
                      "but lacks '%s'") %
                     (node_path, template, props.keys(), e.message))

def GetPropFilename(node_path, props, fname_prop):
  """Create a filename based on the given template and properties

  Args:
    node_path: Path of the node generating this filename (for error
        reporting only)
    props: Dict of properties which can be used in the template:
        key: Variable name
        value: Value of that variable
    fname_prop: Name of property containing the template
  """
  template = props[fname_prop]
  return GetFilename(node_path, props, template)


class CrosConfigImpl(object):
  """The ChromeOS Configuration API for the host.

  CrosConfigImpl is the top level API for accessing ChromeOS Configs from the
  host.

  Properties:
    models: All models in the CrosConfigImpl tree, in the form of a dictionary:
            <model name: string, model: CrosConfigImpl.Node>
    phandle_props: Set of properties which can be phandles (i.e. point to
        another part of the config)
    root: Root node (CrosConigImpl.Node object)
    validator: Validator for the config (CrosConfigValidator object)
  """
  def __init__(self, infile):
    self.infile = infile
    self.models = OrderedDict()
    self.validator = validate_config.GetValidator()
    self.phandle_props = self.validator.GetPhandleProps()

  def _GetProperty(self, absolute_path, property_name):
    """Internal function to read a property from anywhere in the tree

    Args:
      absolute_path: within the root node (e.g. '/chromeos/family/firmware')
      property_name: Name of property to get

    Returns:
      Property object, or None if not found
    """
    return self.root.PathProperty(absolute_path, property_name)

  def GetNode(self, absolute_path):
    """Internal function to get a node from anywhere in the tree

    Args:
      absolute_path: within the root node (e.g. '/chromeos/family/firmware')

    Returns:
      Node object, or None if not found
    """
    return self.root.PathNode(absolute_path)

  def GetFamilyNode(self, relative_path):
    return self.family.PathNode(relative_path)

  def GetFamilyProperty(self, relative_path, property_name):
    """Read a property from a family node

    Args:
      relative_path: Relative path within the family (e.g. '/firmware')
      property_name: Name of property to get

    Returns:
      Property object, or None if not found
    """
    return self.family.PathProperty(relative_path, property_name)

  def GetFirmwareUris(self):
    """Returns a list of (string) firmware URIs.

    Generates and returns a list of firmeware URIs for all model. These URIs
    can be used to pull down remote firmware packages.

    Returns:
      A list of (string) full firmware URIs, or an empty list on failure.
    """
    uris = set()
    for model in self.models.values():
      uris.update(set(model.GetFirmwareUris()))
    return sorted(list(uris))

  def GetTouchFirmwareFiles(self):
    """Get a list of unique touch firmware files for all models

    These files may come from ${FILESDIR} or from a tar file in BCS.

    Returns:
      List of TouchFile objects representing all the touch firmware referenced
      by all models
    """
    file_set = set()
    for model in self.models.values():
      for files in model.GetTouchFirmwareFiles().values():
        file_set.add(files)

    return sorted(file_set, key=lambda files: files.source)

  def GetBcsUri(self, overlay, path):
    """Form a valid BCS URI for downloading files.

    Args:
      overlay: Name of overlay (e.g. 'reef-private')
      path: Path to file in overlay (e.g. 'chromeos-base/'
        'chromeos-touch-firmware-reef/chromeos-touch-firmware-reef-1.0-r9.tbz2')

    Returns:
      Valid BCS URI to download from
    """
    if not overlay.startswith('overlay'):
      return None
    # Strip "overlay-" from bcs_overlay.
    bcs_overlay = overlay[8:]
    return ('gs://chromeos-binaries/HOME/bcs-%(bcs)s/overlay-%(bcs)s/%(path)s' %
            {'bcs': bcs_overlay, 'path': path})

  def GetBspTarFiles(self):
    """Get a list of tarfiles needed by the BSP ebuild

    It is possible to upload tarfiles to BCS which contain firmware used by the
    BSP ebuild. These need to be unpacked so that the files are available to be
    installed by the BSP ebuild.

    The files are stored in distdir by portage, so we can locate them there.

    Returns:
      List of tarfile filenames, each a string
    """
    tarfile_set = set()
    for model in self.models.values():
      for fname in model.GetBspTarFiles().values():
        tarfile_set.add(fname)

    return sorted(tarfile_set)

  def GetArcFiles(self):
    """Get a list of unique Arc++ files for all models

    Returns:
      List of BaseFile objects representing all the arc++ files referenced
      by all models
    """
    file_set = set()
    for model in self.models.values():
      for files in model.GetArcFiles().values():
        file_set.add(files)

    return sorted(file_set, key=lambda files: files.source)

  def GetAudioFiles(self):
    """Get a list of unique audio files for all models

    Returns:
      List of BaseFile objects representing all the audio files referenced
      by all models
    """
    file_set = set()
    for model in self.models.values():
      for files in model.GetAudioFiles().values():
        file_set.add(files)

    return sorted(file_set, key=lambda files: files.source)

  def GetFirmwareBuildTargets(self, target_type):
    """Returns a list of all firmware build-targets of the given target type.

    Args:
      target_type: A string type for the build-targets to return

    Returns:
      A list of all build-targets of the given type, for all models.
    """
    firmware_filter = os.getenv('FW_NAME')
    build_targets = []
    for model in self.models.itervalues():
      node = model.PathNode('/firmware/build-targets')
      # Skip nodes with no build targets
      if not node:
        continue

      # TODO(teravest): Add a name field and use here instead of coreboot.
      key = node.GetStr('coreboot')
      if firmware_filter and key != firmware_filter:
        continue
      target = node.GetStr(target_type)
      if target:
        build_targets.append(target)
      if target_type == 'ec':
        target = node.GetStr('cr50')
        if target:
          build_targets.append(target)
    return sorted(set(build_targets))

  def GetFirmwareBuildCombinations(self, components):
    """Get named firmware build combinations for all models.

    Args:
      components: List of firmware components to get target combinations for.

    Returns:
      OrderedDict containing firmware combinations
        key: combination name
        value: list of firmware targets for specified types

    Raises:
      ValueError if a collision is encountered for named combinations.
    """
    firmware_filter = os.getenv('FW_NAME')

    combos = OrderedDict()
    for model in self.models.itervalues():
      node = model.PathNode('/firmware/build-targets')
      # Skip nodes with no build targets
      if not node:
        continue
      targets = [node.properties.get(c).value for c in components]

      # Always name firmware combinations after the 'coreboot' name.
      # TODO(teravest): Add a 'name' field.
      key = node.GetStr('coreboot')
      if firmware_filter and key != firmware_filter:
        continue

      if key in combos and targets != combos[key]:
        raise ValueError('Colliding firmware combinations found for key %s: '
                         '%s, %s' % (key, targets, combos[key]))
      combos[key] = targets
    return OrderedDict(sorted(combos.iteritems()))

  def GetThermalFiles(self):
    """Get a list of unique thermal files for all models

    Returns:
      List of BaseFile objects representing all the audio files referenced
      by all models
    """
    file_set = set()
    for model in self.models.values():
      for files in model.GetThermalFiles().values():
        file_set.add(files)

    return sorted(file_set, key=lambda files: files.source)

  def ShowTree(self, base_path, tree):
    print('%-10s%s' % ('Size', 'Path'))
    tree.ShowTree(base_path)

  def GetFileTree(self):
    """Get a tree of all files installed by the config

    This looks at all available config that installs files in the root and
    returns them as a tree structure. This can be passed to ShowTree(), which
    is the only feature currently implemented which uses this tree.

    Returns:
        PathComponent object containin the root component
    """
    paths = set()
    for item in self.GetAudioFiles():
      paths.add(item.dest)
    for item in self.GetTouchFirmwareFiles():
      paths.add(item.dest)
      paths.add(item.symlink)
    root = PathComponent('')
    for path in paths:
      root.AddPath(path[1:])

    return root

  @staticmethod
  def GetTargetDirectories():
    """Gets a dict of directory targets for each PropFile property

    Returns:
      Dict:
        key: Property name
        value: Ansolute path for this property
    """
    validator = validate_config.GetValidator()
    return validator.GetTargetDirectories()

  @staticmethod
  def GetPhandleProperties():
    """Gets a dict of properties which contain phandles

    Returns:
      Dict:
        key: Property name
        value: Ansolute path for this property
    """
    validator = validate_config.GetValidator()
    return validator.GetPhandleProps()

  def GetModelList(self):
    """Return a list of models

    Returns:
      List of model names, each a string
    """
    return sorted(self.models.keys())

  def GetFirmwareScript(self):
    """Obtain the packer script to use for this family

    Returns:
      Filename of packer script to use (e.g. 'updater4.sh')
    """
    return self.GetFamilyProperty('/firmware', 'script').value

  def GetFirmwareInfo(self):
    firmware_info = OrderedDict()
    for name in self.GetModelList():
      firmware_info.update(self.models[name].GetFirmwareInfo())
    return firmware_info

  def GetTouchBspUris(self):
    """Get the touch firmware BSP file URI

    Returns:
      URI of touch firmware file to use, or None if none
    """
    file_set = set()
    for model in self.models.values():
      for files in model.GetTouchBspUris().values():
        file_set.add(files)

    return file_set

  def GetBspUris(self):
    """Gets a list of URIs containing files required by the BSP

    This looks through the subsystems which support BCS (Binary Component
    Server) storage and returns a list of URIs that the config needs. Each is
    a tar file which is downloaded from BCS using the SRC_URI mechanism in the
    ebuild. Once it is downloaded, individual files within the archive can
    be accessed and installed.

    Returns:
      List of URIs found (which may be empty)
    """
    return list(self.GetTouchBspUris())

  class Node(object):
    """Represents a single node in the CrosConfig tree, including Model.

    A node can have several subnodes nodes, as well as several properties. Both
    can be accessed via .subnodes and .properties respectively. A few helpers
    are also provided to make node traversal a little easier.

    Properties:
      name: The name of the this node.
      subnodes: Child nodes, in the form of a dictionary:
                <node name: string, child node: CrosConfigImpl.Node>
      properties: All properties attached to this node in the form of a
                  dictionary: <name: string, property: CrosConfigImpl.Property>
    """
    def __init__(self, cros_config):
      self.cros_config = cros_config
      self.subnodes = OrderedDict()
      self.properties = OrderedDict()
      self.default = None
      self.submodels = {}

    def GetPath(self):
      """Get the full path to a node, implemented by subclasses.

      Returns:
        path to node as a string
      """
      return ''

    def FollowShare(self):
      """Follow a node's shares property

      Some nodes support sharing the properties of another node, e.g. firmware
      and whitelabel. This follows that share if it can find it. We don't need
      to be too careful to ignore invalid properties (e.g. whitelabel can only
      be in a model node) since validation takes care of that.

      Returns:
        Node that the share points to, or None if none
      """
      # TODO(sjg@chromium.org):
      # Note that the 'or i in self.subnodes' part is for yaml, where we use a
      # json file within libcros_config_host and this does not support links
      # between nodes (instead every use of a node blows it out fully at that
      # point in the tree). For now this is modelled as a subnode with the
      # name of the phandle, but at some point this will move to merging the
      # target node's properties with this node, so this whole function will
      # become unnecessary.
      share_prop = [i for i in self.cros_config.phandle_props
                    if i in self.properties or i in self.subnodes]
      if share_prop:
        return self.FollowPhandle(share_prop[0])
      return None

    def PathNode(self, relative_path):
      """Returns the CrosConfigImpl.Node at the relative path.

      This method is useful for accessing a nested child object at a relative
      path from a Node (or Model). The path must be separated with '/'
      delimiters. Return None if the path is invalid.

      Args:
        relative_path: A relative path string separated by '/', '/thermal'

      Returns:
        A CrosConfigImpl.Node at the path, or None if it doesn't exist.
      """
      if not relative_path:
        return self
      path_parts = [path for path in relative_path.split('/') if path]
      if not path_parts:
        return self
      part = path_parts[0]
      if part in self.subnodes:
        sub_node = self.subnodes[part]

      # Handle a 'shares' property, which means we can grab nodes / properties
      # from the associated node.
      else:
        shared = self.FollowShare()
        if shared and part in shared.subnodes:
          sub_node = shared.subnodes[part]
        else:
          return None
      return sub_node.PathNode('/'.join(path_parts[1:]))

    def Property(self, property_name):
      """Get a property from a node

      This is a trivial function but it does provide some insulation against our
      internal data structures.

      Args:
        property_name: Name of property to find

      Returns:
        CrosConfigImpl.Property object that waws found, or None if none
      """
      return self.properties.get(property_name)

    def GetStr(self, property_name):
      """Get a string value from a property

      Args:
        property_name: Name of property to access

      Returns:
        String value of property, or '' if not present
      """
      prop = self.Property(property_name)
      return prop.value if prop else ''

    def GetStrList(self, property_name):
      """Get a string-list value from a property

      Args:
        property_name: Name of property to access

      Returns:
        List of strings representing the value of the property, or [] if not
        present
      """
      prop = self.Property(property_name)
      if not prop:
        return []
      return prop.value

    def GetBool(self, property_name):
      """Get a boolean value from a property

      Args:
        property_name: Name of property to access

      Returns:
        True if the property is present, False if not
      """
      return property_name in self.properties

    def PathProperty(self, relative_path, property_name):
      """Returns the value of a property relatative to this node

      This function honours the 'shared' property, by following the phandle and
      searching there, at any component of the path. It also honours the
      'default' property which is defined for nodes.

      Args:
        relative_path: A relative path string separated by '/', e.g. '/thermal'
        property_name: Name of property to look up, e.g 'dptf-dv'

      Returns:
        String value of property, or None if not found
      """
      child_node = self.PathNode(relative_path)
      if not child_node:
        shared = self.FollowShare()
        if shared:
          child_node = shared.PathNode(relative_path)
      if child_node:
        prop = child_node.properties.get(property_name)
        if not prop:
          shared = child_node.FollowShare()
          if shared:
            prop = shared.properties.get(property_name)
        if prop:
          return prop
      if self.default:
        return self.default.PathProperty(relative_path, property_name)
      return None

    def FollowPhandle(self, _prop_name):
      """Follow a property's phandle, implemented by subclasses

      Args:
        _prop_name: Property name to check

      Returns:
        Node that the property's phandle points to, or None if none
      """
      return None

    @staticmethod
    def MergeProperties(props, node, ignore=''):
      if node:
        for name, prop in node.properties.iteritems():
          if (name not in props and not name.endswith('phandle') and
              name != ignore):
            props[name] = prop.value

    def GetMergedProperties(self, node, phandle_prop):
      """Obtain properties in two nodes linked by a phandle

      This is used to create a dict of the properties in a main node along with
      those found in a linked node. The link is provided by a property in the
      main node containing a single phandle pointing to the linked node.

      The result is a dict that combines both nodes' properties, with the
      linked node filling in anything missing. The main node's properties take
      precedence.

      Phandle properties and 'reg' properties are not included. The 'default'
      node is checked as well.

      Args:
        node: Main node to obtain properties from
        phandle_prop: Name of the phandle property to follow to get more
            properties

      Returns:
        dict containing property names and values from both nodes:
          key: property name
          value: property value
      """
      # First get all the property keys/values from the main node
      props = OrderedDict((prop.name, prop.value)
                          for prop in node.properties.values()
                          if prop.name not in [phandle_prop, 'bcs-type', 'reg'])

      # Follow the phandle and add any new ones we find
      self.MergeProperties(props, node.FollowPhandle(phandle_prop), 'bcs-type')
      if self.default:
        # Get the path of this node relative to its model. For example:
        # '/chromeos/models/pyro/thermal' will return '/thermal' in subpath.
        # Once crbug.com/775229 is completed, we will be able to do this in a
        # nicer way.
        _, _, _, _, subpath = node.GetPath().split('/', 4)
        default_node = self.default.PathNode(subpath)
        if default_node:
          self.MergeProperties(props, default_node, phandle_prop)
          self.MergeProperties(props, default_node.FollowPhandle(phandle_prop))
      return OrderedDict(sorted(props.iteritems()))

    def ScanSubnodes(self):
      """Collect a list of submodels"""
      if (self.name in self.cros_config.models and
          'submodels' in self.subnodes.keys()):
        for name, subnode in self.subnodes['submodels'].subnodes.iteritems():
          self.submodels[name] = subnode

    def SubmodelPathProperty(self, submodel_name, relative_path, property_name):
      """Reads a property from a submodel.

      Args:
        submodel_name: Submodel to read from
        relative_path: A relative path string separated by '/'.
        property_name: Name of property to read

      Returns:
        Value of property as a string, or None if not found
      """
      submodel = self.submodels.get(submodel_name)
      if not submodel:
        return None
      return submodel.PathProperty(relative_path, property_name)

    def GetFirmwareUris(self):
      """Returns a list of (string) firmware URIs.

      Generates and returns a list of firmeware URIs for this model. These URIs
      can be used to pull down remote firmware packages.

      Returns:
        A list of (string) full firmware URIs, or an empty list on failure.
      """
      firmware = self.PathNode('/firmware')
      if not firmware or firmware.GetBool('no-firmware'):
        return []
      props = self.GetMergedProperties(firmware, 'shares')

      if 'bcs-overlay' not in props:
        return []
      # Strip "overlay-" from bcs_overlay
      bcs_overlay = props['bcs-overlay'][8:]
      ebuild_name = bcs_overlay.split('-')[0]
      valid_images = [p for n, p in props.iteritems()
                      if n.endswith('-image') and p.startswith('bcs://')]
      # Strip "bcs://" from bcs_from images (to get the file names only)
      file_names = [p[6:] for p in valid_images]
      uri_format = ('gs://chromeos-binaries/HOME/bcs-{bcs}/overlay-{bcs}/'
                    'chromeos-base/chromeos-firmware-{ebuild_name}/{fname}')
      uris = [uri_format.format(bcs=bcs_overlay, model=self.name, fname=fname,
                                ebuild_name=ebuild_name)
              for fname in file_names]
      return sorted(uris)

    def SetupModelProps(self, props):
      props['model'] = self.name
      props['MODEL'] = self.name.upper()

    def GetTouchFirmwareFiles(self):
      """Get a list of unique touch firmware files

      Returns:
        List of TouchFile objects representing the touch firmware referenced
          by this model
      """
      files = {}
      for device_name, props, dirname, tarball in self.GetTouchBspInfo():
        # Add a special property for the capitalised model name
        self.SetupModelProps(props)
        fw_prop_name = 'firmware-bin'
        fw_target_dir = self.cros_config.validator.GetModelTargetDir(
            '/touch/ANY', fw_prop_name)
        if not fw_target_dir:
          raise ValueError(("node '%s': Property '%s' does not have a " +
                            "target directory (internal error)") %
                           (device_name, fw_prop_name))
        sym_prop_name = 'firmware-sym'
        sym_target_dir = self.cros_config.validator.GetModelTargetDir(
            '/touch/ANY', 'firmware-symlink')
        if not sym_target_dir:
          raise ValueError(("node '%s': Property '%s' does not have a " +
                            "target directory (internal error)") %
                           (device_name, sym_prop_name))
        src = GetPropFilename(self.GetPath(), props, fw_prop_name)
        dest = src
        sym_fname = GetPropFilename(self.GetPath(), props, 'firmware-symlink')
        if tarball:
          root, _ = os.path.splitext(os.path.basename(tarball))
          src_dir = os.path.join(root, fw_target_dir[1:])
          src = os.path.join(src_dir, os.path.basename(src))
        else:
          src_dir = dirname
          src = os.path.join(src_dir, src)
        files[device_name] = TouchFile(
            src,
            os.path.join(fw_target_dir, dest),
            os.path.join(sym_target_dir, sym_fname))

      return files

    def GetTouchBspInfo(self, need_filesdir=True):
      distdir = os.getenv('DISTDIR')
      if not distdir:
        raise ValueError('Cannot locate tar files unless DISTDIR is defined')
      filesdir = os.getenv('FILESDIR')
      if not filesdir and need_filesdir:
        raise ValueError('Cannot locate BSP files unless FILESDIR is defined')
      touch = self.PathNode('/touch')
      if touch:
        for device in touch.subnodes.values():
          props = self.GetMergedProperties(device, 'touch-type')
          touch_type = device.FollowPhandle('touch-type')
          bcs = device.FollowPhandle('bcs-type')
          if not bcs and touch_type:
            bcs = touch_type.FollowPhandle('bcs-type')
          if bcs:
            self.MergeProperties(props, bcs)
            tarball = GetPropFilename(touch.GetPath(), props, 'tarball')
            yield [device.name, props, distdir, tarball]
          elif filesdir:
            yield [device.name, props, filesdir, None]

    def GetBspTarFiles(self):
      """Get a dict of tarfiles needed by the BSP ebuild for this model

      Returns:
        Dict of tarfile filenames:
          key: touch device which needs this tarfile
          value: filename of tarfile
      """
      files = {}
      for device_name, _, distdir, tarball in self.GetTouchBspInfo(False):
        if tarball:
          fname = os.path.join(distdir, os.path.basename(tarball))
          files[device_name] = fname
      return files

    def GetTouchBspUris(self):
      """Get a dict of URIs needed by the BSP ebuild for this model

      Returns:
        Dict of URIs:
          key: touch device which needs this URI
          value: URI (string)
      """
      files = {}
      for device_name, props, _, tarball in self.GetTouchBspInfo(False):
        if tarball:
          files[device_name] = self.cros_config.GetBcsUri(props['overlay'],
                                                          tarball)
      return files

    def AllPathNodes(self, relative_path):
      """List all path nodes which match the relative path (including submodels)

      This looks in the model and all its submodels for this relative path.

      Args:
        relative_path: A relative path string separated by '/', '/thermal'

      Returns:
        Dict of:
          key: Name of this model/submodel
          value: Node object for this model/submodel
      """
      path_nodes = {}
      node = self.PathNode(relative_path)
      if node:
        path_nodes[self.name] = node
      for submodel_node in self.submodels.values():
        node = submodel_node.PathNode(relative_path)
        if node:
          path_nodes[submodel_node.name] = node
      return path_nodes

    def GetArcFiles(self):
      """Get a dict of arc++ files

      Returns:
        Dict of BaseFile objects representing the arc++ files referenced
        by this model:
          key: property
          value: BaseFile object
      """
      files = {}
      prop = 'hw-features'
      arc = self.PathNode('/arc')
      target_dir = self.cros_config.validator.GetModelTargetDir('/arc', prop)
      if arc:
        files['base'] = BaseFile(
            arc.properties[prop].value,
            os.path.join(target_dir, arc.properties[prop].value))
      return files

    def GetAudioFiles(self):
      """Get a list of audio files

      Returns:
        Dict of BaseFile objects representing the audio files referenced
        by this model:
          key: (model, property)
          value: BaseFile object
      """
      card = None  # To keep pylint happy since we use it in this function:
      name = ''
      def _AddAudioFile(prop_name, dest_template, dirname=''):
        """Helper to add a single audio file

        If present in the configuration, this adds an audio file containing the
        source and destination file.
        """
        if prop_name in props:
          target_dir = self.cros_config.validator.GetModelTargetDir(
              '/audio/ANY', prop_name)
          if not target_dir:
            raise ValueError(("node '%s': Property '%s' does not have a " +
                              "target directory (internal error)") %
                             (card.name, prop_name))
          files[name, prop_name] = BaseFile(
              GetPropFilename(self.GetPath(), props, prop_name),
              os.path.join(
                  target_dir,
                  dirname,
                  GetFilename(self.GetPath(), props, dest_template)))

      files = {}
      audio_nodes = self.AllPathNodes('/audio')
      for name, audio in audio_nodes.iteritems():
        for card in audio.subnodes.values():
          # First get all the property keys/values from the current node
          props = self.GetMergedProperties(card, 'audio-type')
          self.SetupModelProps(props)

          cras_dir = props.get('cras-config-dir')
          if not cras_dir:
            raise ValueError(("node '%s': Should have a cras-config-dir") %
                             (card.GetPath()))
          _AddAudioFile('volume', '{card}', cras_dir)
          _AddAudioFile('dsp-ini', 'dsp.ini', cras_dir)

          # Allow renaming this file to something other than HiFi.conf
          _AddAudioFile('hifi-conf',
                        os.path.join('{card}.{ucm-suffix}',
                                     os.path.basename(props['hifi-conf'])))
          _AddAudioFile('alsa-conf',
                        '{card}.{ucm-suffix}/{card}.{ucm-suffix}.conf')

          # Non-Intel platforms don't use topology-bin
          topology = props.get('topology-bin')
          if topology:
            _AddAudioFile('topology-bin',
                          os.path.basename(props.get('topology-bin')))

      return files

    def GetThermalFiles(self):
      """Get a dict of thermal files

      Returns:
        Dict of BaseFile objects representing the thermal files referenced
        by this model:
          key: property
          value: BaseFile object
      """
      files = {}
      prop = 'dptf-dv'
      for name, thermal in self.AllPathNodes('/thermal').iteritems():
        target_dir = self.cros_config.validator.GetModelTargetDir('/thermal',
                                                                  prop)
        files[name] = BaseFile(
            thermal.properties[prop].value,
            os.path.join(target_dir, thermal.properties[prop].value))
      return files

    def GetFirmwareInfo(self):
      whitelabel = self.FollowPhandle('whitelabel')
      base_model = whitelabel if whitelabel else self
      firmware_node = self.PathNode('/firmware')
      base_firmware_node = base_model.PathNode('/firmware')

      # If this model shares firmware with another model, get our
      # images from there.
      image_node = base_firmware_node.FollowPhandle('shares')
      if image_node:
        # Override the node - use the shared firmware instead.
        node = image_node
        shared_model = image_node.name
      else:
        node = base_firmware_node
        shared_model = None
      key_id = firmware_node.GetStr('key-id')
      if firmware_node.GetBool('no-firmware'):
        return {}

      have_image = True
      if (whitelabel and base_firmware_node and
          base_firmware_node.Property('sig-id-in-customization-id')):
        # For zero-touch whitelabel devices, we don't need to generate anything
        # since the device will never report this model at runtime. The
        # signature ID will come from customization_id.
        have_image = False

      # Firmware configuration supports both sharing the same fw image across
      # multiple models and pinning specific models to different fw revisions.
      # For context, see:
      # https://chromium.googlesource.com/chromiumos/platform2/+/master/chromeos-config/README.md
      #
      # In order to support this, the firmware build target needs to be
      # decoupled from models (since it can be shared).  This was supported
      # in the config with 'build-targets', which drives the actual firmware
      # build/targets.
      #
      # This takes advantage of that same config to determine what the target
      # FW image will be named in the build output.  This allows a many to
      # many mapping between models and firmware images.
      build_node = node.PathNode('build-targets')
      if build_node:
        bios_build_target = build_node.GetStr('coreboot')
        ec_build_target = build_node.GetStr('ec')
      else:
        bios_build_target, ec_build_target = None, None

      main_image_uri = node.GetStr('main-image')
      main_rw_image_uri = node.GetStr('main-rw-image')
      ec_image_uri = node.GetStr('ec-image')
      pd_image_uri = node.GetStr('pd-image')
      create_bios_rw_image = node.GetBool('create-bios-rw-image')
      extra = node.GetStrList('extra')

      tools = node.GetStrList('tools')

      whitelabels = self.PathNode('/whitelabels')
      if whitelabels or firmware_node.GetBool('sig-id-in-customization-id'):
        sig_id = 'sig-id-in-customization-id'
      else:
        sig_id = self.name

      info = FirmwareInfo(
          self.name, shared_model, key_id, have_image,
          bios_build_target, ec_build_target,
          main_image_uri, main_rw_image_uri, ec_image_uri,
          pd_image_uri, extra, create_bios_rw_image, tools, sig_id)

      # Handle the alternative schema, where whitelabels are in a single model
      # and have whitelabel tags to distinguish them.
      result = OrderedDict()
      result[self.name] = info
      if whitelabels:
        for whitelabel in whitelabels.subnodes.values():
          key_id = whitelabel.GetStr('key-id')
          whitelabel_name = '%s-%s' % (base_model.name, whitelabel.name)
          result[whitelabel_name] = info._replace(
              model=whitelabel_name, key_id=key_id, have_image=False,
              sig_id=whitelabel_name)
      return result

  class Property(object):
    """Represents a single property in a ChromeOS Configuration."""
    def __init__(self):
      pass

    def GetPhandle(self):
      """Get the value of a property as a phandle, implemented by subclasses.

      Returns:
        Property's phandle as an integer (> 0)
      """
      return None


import libcros_config_host_fdt
import v2.libcros_config_host_json

class CrosConfigBoth(object):
  """A class that compares the results of FDT and YAML formats

  This handles checking that calling API methods on both FDT and YAML formats
  return the same results. If not, or if one of the formats is not available,
  an error can be raised, depending on the value of compare_results.
  """
  def __init__(self, fname, compare_results):
    """Constructor for the class

    Properties:
      _method_name: Method to call when _DoMethod() is called. This is updated
          whenever a new API function is requested.

    Args:
      fname: Filename of one of the formats (the filename of the other is
          figured out by replacing the filename extension)
      compare_results: Type of config-format comparison to use (COMPARE_...)
    """
    self._method_name = None
    self._compare_results = compare_results
    base, _ = os.path.splitext(fname)
    fdt_file = base + '.dtb'
    yaml_file = base + '.yaml'
    self.fdt = None
    self.yaml = None
    if os.path.exists(fdt_file):
      with open(fdt_file) as infile:
        self.fdt = libcros_config_host_fdt.CrosConfigFdt(infile)
    if os.path.exists(yaml_file):
      with open(yaml_file) as infile:
        self.yaml = v2.libcros_config_host_json.CrosConfigJson(infile)

    # If we have to have both files, check that now.
    if compare_results == COMPARE_ALWAYS:
      both_files_present = self.fdt and self.yaml
      if not both_files_present:
        raise ValueError('Warning: Could not load both FDT and YAML files (' +
                         "'%s' and '%s')" % (fdt_file, yaml_file))

  def _DoMethod(self, *args):
    """Called to perform an API call into the FDT and YAML implementations

    This calls both FDT and YAML API functions and checks that they return the
    same result. If not, an error is raised.

    Args:
      args: That arguments to pass to the API call

    Returns:
      Result of calling the API function

    Raises:
      ValueError if we don't get the same result from FDT and YAML
    """
    fdt_result, yaml_result = None, None
    if self.fdt:
      fdt_method = getattr(self.fdt, self._method_name)
      fdt_result = fdt_method(*args)
    if self.yaml:
      yaml_method = getattr(self.yaml, self._method_name)
      yaml_result = yaml_method(*args)
    # We don't need to worry about one of the results being missing, since if
    # we care about that it was checked above (see 'both_files_present'). We
    # assume that the methods never return None.
    if fdt_result is not None and yaml_result is not None:
      # We never return Node or Property from any API function, since it is an
      # internal data structure. So we don't need to check equality here. When
      # we do actually return some data, it will be checked.
      result_is_primitive = (
          not isinstance(fdt_result, CrosConfigImpl.Node) and
          not isinstance(fdt_result, CrosConfigImpl.Property))
      if result_is_primitive and fdt_result != yaml_result:
        raise ValueError("Method '%s' results differ: fdt='%s', yaml='%s'",
                         self._method_name, fdt_result, yaml_result)
    # It doesn't matter which we return, since they are equivalent.
    return fdt_result or yaml_result

  def __getattr__(self, name):
    """Returns an object which can be called to execute an API function

    Args:
      name: Name of API function to call

    Returns:
      A method which can be called to execute an API function
    """
    # Handle node.models as a special case. This collects a dict of models to
    # return.
    if name == 'models':
      return {model: NodeBoth(self, '/chromeos/models/' + model)
              for model in self.fdt.models}
    self._method_name = name
    return self._DoMethod


class NodeBoth(object):
  """Models a Node object which has both FDT and yaml versions

  This class internally creates both types of Node and checks that the values
  match.
  """
  def __init__(self, cros_config, path):
    self._path = path
    self._cros_config = cros_config
    self._method_name = None
    self.fdt_node = self._cros_config.fdt.GetNode(self._path)
    self.yaml_node = self._cros_config.yaml.GetNode(self._path)

  def _DoMethod(self, *args):
    """Called to perform an API call into the FDT and YAML implementations

    This calls both FDT and YAML API functions (at the Node level) and checks
    that they return the same result. If not, an error is raised.

    Args:
      args: That arguments to pass to the API call

    Returns:
      Result of calling the API function

    Raises:
      ValueError if we don't get the same result from FDT and YAML
    """
    fdt_method = getattr(self.fdt_node, self._method_name)
    fdt_result = fdt_method(*args)

    yaml_method = getattr(self.yaml_node, self._method_name)
    yaml_result = yaml_method(*args)
    # We don't need to worry about one of the results being missing, since if
    # we care about that, it was checked above (see 'both_files_present'). We
    # assume that the methods never return None.
    # We don't need to compare unless we have two results
    if fdt_result is not None and yaml_result is not None:
      fdt_compare, yaml_compare = fdt_result, yaml_result
      # For properties, we want to compare just the value, not the Property
      # object.
      if isinstance(fdt_compare, CrosConfigImpl.Property):
        fdt_compare = fdt_result.value
        yaml_compare = yaml_result.value
      # If it's a Node, we need to return both types of Node (fdt, yaml)
      if isinstance(fdt_result, CrosConfigImpl.Node):
        return NodeBoth(self._cros_config, fdt_result.GetPath())
      elif fdt_compare != yaml_compare:
        raise ValueError("Method '%s' results differ: fdt='%s', yaml='%s'",
                         self._method_name, fdt_compare, yaml_compare)
    # Once we decide that the results are the same, we can return either one
    return fdt_result or yaml_result

  def __getattr__(self, name):
    """Handles accessing a Node method or member

    Args:
      name: Name of API function to call

    Returns:
      Either
        - A method which can be called to execute an API function
        - For members, the value of that member (either a name or a dict of
            properties and their values)
    """
    # Handle node.name (for yaml) and node.properties as a special case
    if name == 'name':
      return os.path.basename(self._path)
    elif name == 'properties':
      fdt_props = self.fdt_node.properties
      yaml_props = self.yaml_node.properties
      ignore_props = self._cros_config.fdt.phandle_props | set(
          ['default', 'linux,phandle', 'phandle', 'name'])
      fdt_prop_values = {name: fdt_props[name].value
                         for name in fdt_props if name not in ignore_props}
      yaml_prop_values = {name: yaml_props[name].value
                          for name in yaml_props if name not in ignore_props}
      if fdt_prop_values != yaml_prop_values:
        raise ValueError("Properties for '%s' differ: fdt='%s', yaml='%s'",
                         self._path, fdt_prop_values, yaml_prop_values)
      return fdt_props
    self._method_name = name
    return self._DoMethod


def CrosConfig(fname=None, config_format=None,
               compare_results=COMPARE_IF_PRESENT):
  """Create a new CrosConfigImpl object

  This is in a separate function to allow us to (in the future) support YAML,
  which will have a different means of creating the impl class.

  Args:
    fname: Filename of config file
    config_format: Configuration format to use (FORMAT_...)
    compare_results: Type of config-format comparison to use (COMPARE_...)
  """
  if config_format is None:
    if fname and ('.yaml' in fname or '.json' in fname):
      config_format = FORMAT_YAML
    else:
      config_format = FORMAT_FDT
  if not fname:
    if 'SYSROOT' not in os.environ:
      raise ValueError('No master configuration is available outside the '
                       'ebuild environemnt. You must specify one')
    fname = os.path.join(
        os.environ['SYSROOT'], UNIBOARD_DTB_INSTALL_DIR,
        'config.' + ('dtb' if config_format == FORMAT_FDT else 'yaml'))
  if fname == '-':
    if compare_results == COMPARE_ALWAYS:
      raise ValueError('Cannot compare results from stdin')
    infile = sys.stdin
  elif compare_results:
    return CrosConfigBoth(fname, compare_results)
  else:
    infile = open(fname)

  if config_format == FORMAT_FDT:
    return libcros_config_host_fdt.CrosConfigFdt(infile)
  elif config_format == FORMAT_YAML:
    return v2.libcros_config_host_json.CrosConfigJson(infile)
  else:
    raise ValueError("Invalid config format '%s' requested" % config_format)
