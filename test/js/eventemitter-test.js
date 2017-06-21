const expect = require('chai').expect
const testRoot = require('path').resolve(__dirname, '..')
const bindings = require('bindings')({ module_root: testRoot, bindings: 'eventemitter' })

describe('Verify EventEmitter Single', function() {
    it('should invoke the callback for test', function(done) {
        let thing = new bindings.EmitterThing()
        let n = 100;
        let k = 0;
        thing.on('test', function(ev) { 
            expect(ev).to.be.a('string')
            expect(ev).to.equal('Test' + k++)
            if(k === n) {
                done();
            }
        })

        thing.run(n)
    })
})

describe('Verify EventEmitter Multi', function() {
    it('should invoke the callbacks for test, test2, and test3', function(done) {
        let thing = new bindings.EmitterThing()
        let n = 300;
        let k = [0,0,0];
        thing.on('test', function(ev) { 
            expect(ev).to.be.a('string')
            expect(ev).to.equal('Test' + k[0]++)
        })

        thing.on('test2', function(ev) {
            expect(ev).to.be.a('string')
            expect(ev).to.equal('Test' + k[1]++) 
        })
        thing.on('test3', function(ev) {
            expect(ev).to.be.a('string')
            expect(ev).to.equal('Test' + k[2]++)

            if(k[2] === n) {
                while(k[0] !== n || k[1] !== n);
                done();
            }
        })
        thing.run(n)
    })
})


describe('Verify EventEmitter Reentrant Single', function() {
    it('should invoke the callback for test', function(done) {
        let thing = new bindings.EmitterThing()
        let n = 100;
        let k = 0;
        thing.on('test', function(ev) {
            expect(ev).to.be.a('string')
            expect(ev).to.equal('Test' + k++)
            if(k === n) {
                done();
            }
        })

        thing.runReentrant(n)
    })
})

describe('Verify EventEmitter Reentrant Multi', function() {
    it('should invoke the callbacks for test, test2, and test3', function(done) {
        let thing = new bindings.EmitterThing()
        let n = 300;
        let k = [0,0,0];
        thing.on('test', function(ev) {
            expect(ev).to.be.a('string')
            expect(ev).to.equal('Test' + k[0]++)
        })

        thing.on('test2', function(ev) {
            expect(ev).to.be.a('string')
            expect(ev).to.equal('Test' + k[1]++)
        })
        thing.on('test3', function(ev) {
            expect(ev).to.be.a('string')
            expect(ev).to.equal('Test' + k[2]++)

            if(k[2] === n) {
                while(k[0] !== n || k[1] !== n);
                done();
            }
        })
        thing.runReentrant(n)
    })
})




