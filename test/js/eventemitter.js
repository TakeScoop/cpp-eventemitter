'use strict'

const expect = require('code').expect
const testRoot = require('path').resolve(__dirname, '..')
const bindings = require('bindings')({ 'module_root': testRoot, bindings: 'eventemitter' })
const iterate = require('leakage').iterate

describe('Verify EventEmitter Single', function() {
    it('should invoke the callback for test', function(done) {
        let thing = new bindings.EmitterThing()
        let n = 100
        let k = 0
        thing.on('test', function(ev) { 
            expect(ev).to.be.a.string()
            expect(ev).to.equal('Test' + k++)
            if (k === n) {
                done()
            }
        })

        thing.run(n)
    })
})

describe('Verify EventEmitter Multi', function() {
    it('should invoke the callbacks for test, test2, and test3', function(done) {
        let thing = new bindings.EmitterThing()
        let n = 300
        let k = [0, 0, 0]
        thing.on('test', function(ev) { 
            expect(ev).to.be.a.string()
            expect(ev).to.equal('Test' + k[0]++)
        })

        thing.on('test2', function(ev) {
            expect(ev).to.be.a.string()
            expect(ev).to.equal('Test' + k[1]++) 
        })
        thing.on('test3', function(ev) {
            expect(ev).to.be.a.string()
            expect(ev).to.equal('Test' + k[2]++)

            if (k[2] === n) {
                while (k[0] !== n || k[1] !== n) { /* do nothing */ }
                done()
            }
        })
        thing.run(n)
    })
})


describe('Verify EventEmitter Reentrant Single', function() {
    it('should invoke the callback for test', function(done) {
        let thing = new bindings.EmitterThing()
        let n = 100
        let k = 0
        thing.on('test', function(ev) {
            expect(ev).to.be.a.string()
            expect(ev).to.equal('Test' + k++)
            if (k === n) {
                done()
            }
        })

        thing.runReentrant(n)
    })
})

describe('Verify EventEmitter Reentrant Multi', function() {
    it('should invoke the callbacks for test, test2, and test3', function(done) {
        let thing = new bindings.EmitterThing()
        let n = 300
        let k = [0, 0, 0]
        thing.on('test', function(ev) {
            expect(ev).to.be.a.string()
            expect(ev).to.equal('Test' + k[0]++)
        })

        thing.on('test2', function(ev) {
            expect(ev).to.be.a.string()
            expect(ev).to.equal('Test' + k[1]++)
        })
        thing.on('test3', function(ev) {
            expect(ev).to.be.a.string()
            expect(ev).to.equal('Test' + k[2]++)

            if (k[2] === n) {
                while (k[0] !== n || k[1] !== n) { /* do nothing */ }
                done()
            }
        })
        thing.runReentrant(n)
    })
})


describe('Verify callback memory is reclaimed', function() {
    it('Should not increase memory usage over time', function(done) {
        this.timeout(15000)
        iterate(() => {
            let thing = new bindings.EmitterThing()
            let v = new Buffer(1000)

            thing.on('test', function(ev) {
                v.compare(new Buffer(1000))
            })
        })
        done()
    })
})




