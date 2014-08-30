import config.package
import os, sys

class Configure(config.package.Package):
  def __init__(self, framework):
    config.package.Package.__init__(self, framework)
    self.download = ['http://launchpad.net/fiat/0.x/0.9.9/+download/fiat-0.9.9.tar.gz'] # 'http://ftp.mcs.anl.gov/pub/petsc/externalpackages/fiat-dev.tar.gz'
    self.downloadonWindows = 1
    self.liblist           = [['Lagrange.py']]
    self.libdir            = os.path.join('lib', 'python', 'site-packages')
    self.altlibdir         = os.path.join('lib', 'python'+'.'.join(map(str, sys.version_info[0:2])), 'site-packages')
    return

  def setupDependencies(self, framework):
    config.package.Package.setupDependencies(self, framework)
    self.scipy = framework.require('config.packages.scientificpython', self)
    self.deps  = [self.scipy]
    return

  def Install(self):
    import shutil
    # We could make a check of the md5 of the current configure framework
    self.logPrintBox('Installing FIAT')
    installLoc = os.path.join(self.installDir, self.altlibdir)
    packageDir = os.path.join(installLoc, 'FIAT')
    if self.installSudo:
      self.installDirProvider.printSudoPasswordMessage()
      try:
        output,err,ret  = config.base.Configure.executeShellCommand(self.installSudo+'mkdir -p '+installLoc+' && '+self.installSudo+'rm -rf '+packageDir+'  && '+self.installSudo+'cp -rf '+os.path.join(self.packageDir, 'FIAT')+' '+packageDir, timeout=6000, log = self.framework.log)
      except RuntimeError, e:
        raise RuntimeError('Error copying FIAT files from '+os.path.join(self.packageDir, 'FIAT')+' to '+packageDir)
    else:
      if not os.path.isdir(installLoc):
        os.makedirs(installLoc)
      if os.path.exists(packageDir):
        shutil.rmtree(packageDir)
      shutil.copytree(os.path.join(self.packageDir, 'FIAT'), packageDir)
    self.framework.actions.addArgument('FIAT', 'Install', 'Installed FIAT into '+self.installDir)
    return self.installDir

  def configureLibrary(self):
    '''Find an installation ando check if it can work with PETSc'''
    import sys
    self.framework.log.write('==================================================================================\n')
    self.framework.log.write('Checking for a functional '+self.name+'\n')

    self.checkDependencies()
    for location, rootDir, lib, incDir in self.generateGuesses():
      try:
        libDir = os.path.dirname(lib[0])
        self.framework.logPrint('Checking location '+location)
        self.framework.logPrint('Added directory '+libDir+' to Python path')
        sys.path.insert(0, libDir)
        import FIAT
        from FIAT import ufc_simplex
        import FIAT.lagrange
        import FIAT.quadrature
        self.found = 1
        if not hasattr(self.framework, 'packages'):
          self.framework.packages = []
        self.framework.packages.append(self)
        return
      except ImportError, e:
        self.framework.logPrint('ERROR: Could not import FIAT: '+str(e))
    raise RuntimeError('Could not find a functional '+self.name+'\n')
