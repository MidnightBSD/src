pipeline {
    parameters {
        choice(name: 'ARCHITECTURE_FILTER', choices: ['all', 'amd64', 'i386'], description: 'Run on specific architecture')
    }
    agent none
    stages {
        stage('Check Branch Name') {
            steps {
                script {
                    if (env.BRANCH_NAME.startsWith('vendor/')) {
                        currentBuild.result = 'NOT_BUILT'
                        error('Skipping build for vendor branch')
                    }
                }
            }
        }
        stage('BuildAndTest') {
            matrix {
                agent {
                    label "${ARCHITECTURE} && bsd"
                }
                
                when { anyOf {
                    expression { params.ARCHITECTURE_FILTER == 'all' }
                    expression { params.ARCHITECTURE_FILTER == env.ARCHITECTURE }
                } }
                axes {
                    axis {
                        name 'ARCHITECTURE'
                        values 'amd64', 'i386'
                    }
                }
                stages {
                    stage('Prepare') {
                        environment {
                            MAKEOBJDIRPREFIX = "${env.WORKSPACE}/obj"
                            DESTDIR = "${env.WORKSPACE}/destdir/${ARCHITECTURE}"
                        }
                        steps {
                            echo "Prepare for ${ARCHITECTURE}"
                            sh "mkdir -p ${MAKEOBJDIRPREFIX}"
                            sh "rm -rf ${DESTDIR}"
                            sh "mkdir -p ${DESTDIR}"
                            sh 'make clean' 
                        }
                    }
                    stage('buildworld') {
                        environment {
                            MAKEOBJDIRPREFIX = "${env.WORKSPACE}/obj"
                            DESTDIR = "${env.WORKSPACE}/destdir/${ARCHITECTURE}"
                        }
                        steps {
                            catchError(buildResult: 'SUCCESS', stageResult: 'FAILURE') {
                                echo "Do buildworld for ${ARCHITECTURE}"
                                sh 'make -j5 buildworld'
                            }
                        }
                    }
                    stage('buildkernel') {
                        environment {
                            MAKEOBJDIRPREFIX = "${env.WORKSPACE}/obj"
                            DESTDIR = "${env.WORKSPACE}/destdir/${ARCHITECTURE}"
                        }
                        steps {
                            echo "Do buildkernel for ${ARCHITECTURE}"
                             sh 'make -j5 buildkernel' 
                        }
                    }
                    stage('installworld') {
                        environment {
                            MAKEOBJDIRPREFIX = "${env.WORKSPACE}/obj"
                            DESTDIR = "${env.WORKSPACE}/destdir/${ARCHITECTURE}"
                        }
                        steps {
                            echo "Do installworld for ${ARCHITECTURE}"
                            sh 'make installworld DESTDIR=${DESTDIR}'
                            sh 'make distribution DESTDIR=${DESTDIR}'
                        }
                    }
                    stage('installkernel') {
                        environment {
                            MAKEOBJDIRPREFIX = "${env.WORKSPACE}/obj"
                            DESTDIR = "${env.WORKSPACE}/destdir/${ARCHITECTURE}"
                        }
                        steps {
                            echo "Do installkernel for ${ARCHITECTURE}"
                            sh 'make installkernel DESTDIR=${DESTDIR}'
                        }
                    }
                    stage('tests-install') {
                        environment {
                            MAKEOBJDIRPREFIX = "${env.WORKSPACE}/obj"
                            DESTDIR = "${env.WORKSPACE}/destdir/${ARCHITECTURE}"
                        }
                        steps {
                            echo "Do tests-install for ${ARCHITECTURE}"
                            sh 'make tests-install DESTDIR=${DESTDIR}'
                        }
                    }
                    stage('tests') {
                        environment {
                            DESTDIR = "${env.WORKSPACE}/destdir/${ARCHITECTURE}"
                            JAIL_NAME = "jenkins-${ARCHITECTURE}-${env.BUILD_NUMBER}"
                            KYUA_RESULTS = "${env.WORKSPACE}/kyua-results-${ARCHITECTURE}.db"
                            JUNIT_RESULTS = "junit-results-${ARCHITECTURE}.xml"
                        }
                        steps {
                            echo "Do tests for ${ARCHITECTURE}"
                            sh "rm -f ${KYUA_RESULTS} ${JUNIT_RESULTS}"
                            sh "mkdir -p ${DESTDIR}/dev"
                            sh "mount -t devfs devfs ${DESTDIR}/dev"
                            sh "jail -c name=${JAIL_NAME} path=${DESTDIR} host.hostname=${JAIL_NAME} persist"
                            sh "jexec ${JAIL_NAME} /usr/bin/kyua test -k /usr/tests/Kyuafile --results-file /kyua-results-${ARCHITECTURE}.db"
                            sh "cp ${DESTDIR}/kyua-results-${ARCHITECTURE}.db ${KYUA_RESULTS}"
                            sh "kyua report-junit --output ${JUNIT_RESULTS} --results-file ${KYUA_RESULTS}"
                        }
                        post {
                            always {
                                sh "jail -r ${JAIL_NAME} || true"
                                sh "umount ${DESTDIR}/dev || true"
                                junit allowEmptyResults: true, testResults: "${JUNIT_RESULTS}"
                            }
                        }
                    }
                }
            }
        }
    }
}
