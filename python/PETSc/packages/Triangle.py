import config.base

class Configure(config.base.Configure):
  def __init__(self, framework):
    config.base.Configure.__init__(self, framework)
    self.headerPrefix = ''
    self.substPrefix  = ''
    self.compilers    = self.framework.require('config.compilers', self)
    self.name         = 'Triangle'
    self.PACKAGE      = self.name.upper()
    self.package      = self.name.lower()
    return

  def configureLibrary(self):
    '''Find a Triangle installation and check if it can work with PETSc'''
    return

  def setOutput(self):
    #self.addDefine('HAVE_TRIANGLE', 0)
    self.addSubstitution('TRIANGLE_INCLUDE', '')
    self.addSubstitution('TRIANGLE_LIB', '')
#    self.framework.packages.append(self)
    return

  def configure(self):
    self.executeTest(self.configureLibrary)
    self.setOutput()
    return
