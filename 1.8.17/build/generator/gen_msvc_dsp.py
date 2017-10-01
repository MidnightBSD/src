#
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#
#
#
# gen_dsp.py -- generate Microsoft Visual C++ 6 projects
#

import os
import sys

import gen_base
import gen_win
import ezt


class Generator(gen_win.WinGeneratorBase):
  "Generate a Microsoft Visual C++ 6 project"

  def __init__(self, fname, verfname, options):
    gen_win.WinGeneratorBase.__init__(self, fname, verfname, options,
                                      'msvc-dsp')

  def quote(self, str):
    return '"%s"' % str

  def write_project(self, target, fname):
    "Write a Project (.dsp)"

    if isinstance(target, gen_base.TargetExe):
      targtype = "Win32 (x86) Console Application"
      targval = "0x0103"
    elif isinstance(target, gen_base.TargetJava):
        targtype = "Win32 (x86) Generic Project"
        targval = "0x010a"
    elif isinstance(target, gen_base.TargetLib):
      if target.msvc_static:
        targtype = "Win32 (x86) Static Library"
        targval = "0x0104"
      else:
        targtype = "Win32 (x86) Dynamic-Link Library"
        targval = "0x0102"
    elif isinstance(target, gen_base.TargetProject):
      if target.cmd:
        targtype = "Win32 (x86) External Target"
        targval = "0x0106"
      else:
        targtype = "Win32 (x86) Generic Project"
        targval = "0x010a"
    else:
      raise gen_base.GenError("Cannot create project for %s" % target.name)

    target.output_name = self.get_output_name(target)
    target.output_dir = self.get_output_dir(target)
    target.intermediate_dir = self.get_intermediate_dir(target)
    target.output_pdb = self.get_output_pdb(target)

    configs = self.get_configs(target)

    sources = self.get_proj_sources(True, target)

    data = {
      'target' : target,
      'target_type' : targtype,
      'target_number' : targval,
      'rootpath' : self.rootpath,
      'platforms' : self.platforms,
      'configs' : configs,
      'includes' : self.get_win_includes(target),
      'sources' : sources,
      'default_platform' : self.platforms[0],
      'default_config' : configs[0].name,
      'is_exe' : ezt.boolean(isinstance(target, gen_base.TargetExe)),
      'is_external' : ezt.boolean((isinstance(target, gen_base.TargetProject)
                                   or isinstance(target, gen_base.TargetI18N))
                                  and target.cmd),
      'is_utility' : ezt.boolean(isinstance(target,
                                            gen_base.TargetProject)),
      'is_dll' : ezt.boolean(isinstance(target, gen_base.TargetLib)
                             and not target.msvc_static),
      'instrument_apr_pools' : self.instrument_apr_pools,
      'instrument_purify_quantify' : self.instrument_purify_quantify,
      }

    self.write_with_template(fname, 'templates/msvc_dsp.ezt', data)

  def write(self):
    "Write a Workspace (.dsw)"

    # Gather sql targets for inclusion in svn_config project.
    class _eztdata(object):
      def __init__(self, **kw):
        vars(self).update(kw)

    import sys
    sql=[]
    for hdrfile, sqlfile in sorted(self.graph.get_deps(gen_base.DT_SQLHDR),
                                   key=lambda t: t[0]):
      sql.append(_eztdata(header=hdrfile.replace('/', '\\'),
                          source=sqlfile[0].replace('/', '\\'),
                          svn_python=sys.executable))

    self.move_proj_file(self.projfilesdir,
                        'svn_config.dsp',
                          (
                            ('sql', sql),
                            ('project_guid', self.makeguid('__CONFIG__')),
                          )
                        )
    self.move_proj_file(self.projfilesdir,
                        'svn_locale.dsp',
                        (
                          ('project_guid', self.makeguid('svn_locale')),
                        ))
    self.write_zlib_project_file('zlib.dsp')
    self.write_serf_project_file('serf.dsp')
    install_targets = self.get_install_targets()

    targets = [ ]

    self.gen_proj_names(install_targets)

    # Traverse the targets and generate the project files
    for target in install_targets:
      name = target.name
      fname = self.get_external_project(target, 'dsp')
      if fname is None:
        fname = os.path.join(self.projfilesdir,
                             "%s_msvc.dsp" % target.proj_name)
        self.write_project(target, fname)

      if '-' in fname:
        fname = '"%s"' % fname

      depends = [ ]
      if not isinstance(target, gen_base.TargetI18N):
        depends = self.adjust_win_depends(target, name)
	#print name
	#for dep in depends:
	#  print "	",dep.name

      dep_names = [ ]
      for dep in depends:
        dep_names.append(dep.proj_name)

      targets.append(
        gen_win.ProjectItem(name=target.proj_name,
                            dsp=fname.replace(os.sep, '\\'),
                            depends=dep_names))

    targets.sort(key = lambda x: x.name)
    data = {
      'targets' : targets,
      }

    self.write_with_template('subversion_msvc.dsw', 'templates/msvc_dsw.ezt', data)
